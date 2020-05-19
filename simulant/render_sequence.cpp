//
//   Copyright (c) 2011-2017 Luke Benstead https://simulant-engine.appspot.com
//
//     This file is part of Simulant.
//
//     Simulant is free software: you can redistribute it and/or modify
//     it under the terms of the GNU Lesser General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Simulant is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU Lesser General Public License for more details.
//
//     You should have received a copy of the GNU Lesser General Public License
//     along with Simulant.  If not, see <http://www.gnu.org/licenses/>.
//

#include <unordered_map>

#include "generic/algorithm.h"
#include "render_sequence.h"
#include "stage.h"
#include "nodes/actor.h"
#include "nodes/camera.h"
#include "nodes/light.h"

#include "meshes/mesh.h"
#include "window.h"
#include "partitioner.h"
#include "loader.h"

namespace smlt {

RenderSequence::RenderSequence(Window *window):
    window_(window),
    renderer_(window->renderer.get()) {

    //Set up the default render options
    render_options.wireframe_enabled = false;
    render_options.texture_enabled = true;
    render_options.backface_culling_enabled = true;
    render_options.point_size = 1;
}

RenderSequence::~RenderSequence() {
    clean_up_connection_.disconnect();
    destroy_all_pipelines();
}

PipelinePtr RenderSequence::find_pipeline(const std::string &name) {
    for(auto& pipeline: ordered_pipelines_) {
        if(pipeline->name() == name) {
            return pipeline;
        }
    }

    return nullptr;
}

bool RenderSequence::destroy_pipeline(const std::string& name) {
    auto pip = find_pipeline(name);
    if(!pip) {
        return false;
    }

    if(queued_for_destruction_.count(pip)) {
        return false;
    }

    queued_for_destruction_.insert(pip);

    /* When a user requests destruction, we deactivate immediately
     * as that's the path of least surprise. The pipeline won't be used
     * anyway on the next render, this just makes sure that the stage for example
     * doesn't think it's part of an active pipeline until then */
    pip->deactivate();

    return true;
}

void RenderSequence::destroy_pipeline_immediately(const std::string& name) {
    auto pip = find_pipeline(name);
    pip->deactivate();

    queued_for_destruction_.erase(pip);
    ordered_pipelines_.remove(pip);

    pool_.erase(
        std::remove_if(pool_.begin(), pool_.end(), [name](const Pipeline::ptr& pip) -> bool {
            return pip->name() == name;
        }),
        pool_.end()
    );
}

void RenderSequence::clean_up() {
    for(auto pip: queued_for_destruction_) {
        pip->deactivate();
        ordered_pipelines_.remove(pip);

        auto name = pip->name();
        pool_.erase(
            std::remove_if(pool_.begin(), pool_.end(), [name](const Pipeline::ptr& pip) -> bool {
                return pip->name() == name;
            }),
            pool_.end()
        );
    }
    queued_for_destruction_.clear();
}

void RenderSequence::destroy_all_pipelines() {
    auto pipelines = ordered_pipelines_;
    for(auto pip: pipelines) {
        assert(pip);
        destroy_pipeline(pip->name());
    }
}

bool RenderSequence::has_pipeline(const std::string& name) {
    return bool(find_pipeline(name));
}

void RenderSequence::sort_pipelines() {
    auto do_sort = [&]() {
        ordered_pipelines_.sort(
            [](PipelinePtr lhs, PipelinePtr rhs) { return lhs->priority() < rhs->priority(); }
        );
    };

    do_sort();
}

PipelinePtr RenderSequence::new_pipeline(
    const std::string& name, StageID stage, CameraID camera,
    const Viewport& viewport, TextureID target, int32_t priority) {

    auto pipeline = Pipeline::create(
        this, name, stage, camera
    );

    /* New pipelines should always start deactivated to avoid the attached stage
     * as being updated automatically in the main thread when the pipeline
     * is constructed */
    pipeline->deactivate();
    pipeline->set_viewport(viewport);
    pipeline->set_target(target);
    pipeline->set_priority(priority);

    pool_.push_back(pipeline);

    ordered_pipelines_.push_back(pipeline.get());
    sort_pipelines();

    return pipeline.get();
}

void RenderSequence::set_renderer(Renderer* renderer) {
    renderer_ = renderer;
}

void RenderSequence::run() {
    clean_up();  /* Clean up any destroyed pipelines before rendering */

    targets_rendered_this_frame_.clear();

    /* Perform any pre-rendering tasks */
    renderer_->pre_render();

    int actors_rendered = 0;
    for(auto& pipeline: ordered_pipelines_) {
        run_pipeline(pipeline, actors_rendered);
    }

    window->stats->set_subactors_rendered(actors_rendered);
}


uint64_t generate_frame_id() {
    static uint64_t frame_id = 0;
    return ++frame_id;
}

void RenderSequence::run_pipeline(PipelinePtr pipeline_stage, int &actors_rendered) {
    /*
     * This is where rendering actually happens.
     *
     * FIXME: This needs some serious thought regarding thread-safety. There is no locking here
     * and another thread could be adding/removing objects, updating the partitioner, or changing materials
     * and/or textures on renderables. We need to make sure that we render a consistent snapshot of the world
     * which means figuring out some kind of locking around the render queue building and traversal, or
     * some deep-copying (of materials/textures/renderables) to make sure that nothing changes during traversal
     */
    uint64_t frame_id = generate_frame_id();

    if(!pipeline_stage->is_active()) {
        return;
    }

    auto stage = pipeline_stage->stage();
    auto camera = pipeline_stage->camera();

    if(!stage || !camera) {
        L_DEBUG("Stage or camera has been destroyed, disabling pipeline");
        pipeline_stage->deactivate();
        return;
    }

    RenderTarget& target = *window_; //FIXME: Should be window or texture

    /*
     *  Render targets can specify whether their buffer should be cleared at the start of each frame. We do this the first
     *  time we hit a render target when processing the pipelines. We keep track of the targets that have been rendered each frame
     *  and this list is cleared at the start of run().
     */
    if(targets_rendered_this_frame_.find(&target) == targets_rendered_this_frame_.end()) {
        if(target.clear_every_frame_flags()) {
            Viewport view(smlt::VIEWPORT_TYPE_FULL, target.clear_every_frame_colour());
            view.clear(target, target.clear_every_frame_flags());
        }

        targets_rendered_this_frame_.insert(&target);
    }

    auto& viewport = pipeline_stage->viewport;

    uint32_t clear = pipeline_stage->clear_flags();
    if(clear) {
        viewport->clear(target, clear); //Implicitly calls apply
    } else {
        viewport->apply(target); //FIXME apply shouldn't exist, it ties Viewport to OpenGL...
    }

    signal_pipeline_started_(*pipeline_stage);

    // Trigger a signal to indicate the stage is about to be rendered
    stage->signal_stage_pre_render()(camera->id(), viewport);

    // Apply any outstanding writes to the partitioner
    stage->partitioner->_apply_writes();

    static std::vector<LightID> light_ids;
    static std::vector<StageNode*> nodes_visible;

    /* Empty out, but leave capacity to prevent constant allocations */
    light_ids.resize(0);
    nodes_visible.resize(0);

    // Gather the lights and geometry visible to the camera
    stage->partitioner->lights_and_geometry_visible_from(camera->id(), light_ids, nodes_visible);

    // Get the actual lights from the IDs
    auto lights_visible = map<decltype(light_ids), std::vector<LightPtr>>(
        light_ids, [&](const LightID& light_id) -> LightPtr { return stage->light(light_id); }
    );

    // Reset it, ready for this pipeline
    render_queue_.reset(stage, window->renderer.get(), camera);

    // Mark the visible objects as visible
    for(auto& node: nodes_visible) {
        if(!node->is_visible()) {
            continue;
        }

        auto renderable_lights = filter(lights_visible, [&node](const LightPtr& light) -> bool {
            // Filter by whether or not the renderable bounds intersects the light bounds
            if(light->type() == LIGHT_TYPE_DIRECTIONAL) {
                return true;
            } else if(light->type() == LIGHT_TYPE_SPOT_LIGHT) {
                return node->transformed_aabb().intersects_aabb(light->transformed_aabb());
            } else {
                return node->transformed_aabb().intersects_sphere(light->absolute_position(), light->range() * 2);
            }
        });

        std::partial_sort(
            renderable_lights.begin(),
            renderable_lights.begin() + std::min(MAX_LIGHTS_PER_RENDERABLE, (uint32_t) renderable_lights.size()),
            renderable_lights.end(),
            [=](LightPtr lhs, LightPtr rhs) {
                /* FIXME: Sorting by the centre point is problematic. A renderable is made up
                 * of many polygons, by choosing the light closest to the center you may find that
                 * that polygons far away from the center aren't affected by lights when they should be.
                 * This needs more thought, probably. */
                if(lhs->type() == LIGHT_TYPE_DIRECTIONAL && rhs->type() != LIGHT_TYPE_DIRECTIONAL) {
                    return true;
                } else if(rhs->type() == LIGHT_TYPE_DIRECTIONAL && lhs->type() != LIGHT_TYPE_DIRECTIONAL) {
                    return false;
                }

                float lhs_dist = (node->centre() - lhs->position()).length_squared();
                float rhs_dist = (node->centre() - rhs->position()).length_squared();
                return lhs_dist < rhs_dist;
            }
        );

        /* FIXME: Store squared distances to avoid sqrt */
        float distance_to_camera = (camera->absolute_position() - node->absolute_position()).length();

        /* Find the ideal detail level at this distance from the camera */
        auto level = pipeline_stage->detail_level_at_distance(distance_to_camera);

        /* Push any renderables for this node */
        auto initial = render_queue_.renderable_count();
        node->_get_renderables(&render_queue_, camera, level);

        // FIXME: Change _get_renderables to return the number inserted
        auto count = render_queue_.renderable_count() - initial;

        for(auto i = initial; i < initial + count; ++i) {
            auto renderable = render_queue_.renderable(i);

            assert(
                renderable->arrangement == MESH_ARRANGEMENT_LINES ||
                renderable->arrangement == MESH_ARRANGEMENT_LINE_STRIP ||
                renderable->arrangement == MESH_ARRANGEMENT_QUADS ||
                renderable->arrangement == MESH_ARRANGEMENT_TRIANGLES ||
                renderable->arrangement == MESH_ARRANGEMENT_TRIANGLE_FAN ||
                renderable->arrangement == MESH_ARRANGEMENT_TRIANGLE_STRIP
            );

            assert(renderable->material);
            assert(renderable->index_data);
            assert(renderable->vertex_data);

            renderable->light_count = renderable_lights.size();
            for(auto i = 0u; i < renderable->light_count; ++i) {
                renderable->lights_affecting_this_frame[i] = renderable_lights[i];
            }
        }
    }

    actors_rendered += render_queue_.renderable_count();

    using namespace std::placeholders;

    auto visitor = renderer_->get_render_queue_visitor(camera);

    // Render the visible objects
    render_queue_.traverse(visitor.get(), frame_id);

    // Trigger a signal to indicate the stage has been rendered
    stage->signal_stage_post_render()(camera->id(), viewport);

    signal_pipeline_finished_(*pipeline_stage);
    render_queue_.clear();
}

}
