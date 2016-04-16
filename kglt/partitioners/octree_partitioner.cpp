#include "octree_partitioner.h"

#include "../stage.h"
#include "../light.h"
#include "../actor.h"
#include "../camera.h"
#include "../particles.h"

/*
 * TODO:
 *
 * The tree stores a raw pointer to the boundable entity - surely this needs to be refcounted? What if the
 * entity is destroyed in another thread? There's so much to fix in this rendering system :/
 */

namespace kglt {

void OctreePartitioner::event_actor_changed(ActorID ent) {
    L_DEBUG("Actor changed, updating partitioner");
    remove_actor(ent);
    add_actor(ent);
}

void OctreePartitioner::add_particle_system(ParticleSystemID ps) {
    auto system = stage->particle_system(ps); //Get a handle on the particle system

    BoundableEntity* ent = system.__object.get(); //Get the raw pointer as a BoundableEntity (bad?)
    tree_.grow(ent);
}

void OctreePartitioner::remove_particle_system(ParticleSystemID ps) {
    auto system = stage->particle_system(ps);
    BoundableEntity* ent = system.__object.get(); //Get the raw pointer as a BoundableEntity (bad?)
    tree_.shrink(ent);
}

void OctreePartitioner::add_geom(GeomID geom_id) {
    auto geom = stage->geom(geom_id);

    /*
     * 1. Iterate through the polygons in the geom, for each one generate a BoundableEntity
     * 2. Insert into the tree, but set a callback for grow()
     * 3. The callback must stash a new static chunk (if it doesn't exist on the node) otherwise
     *    get a handle to the existing one.
     * 4. Add the polygon to the static chunk, return false from the callback so that the octree
     *    doesn't add the polygon to the object list on the node
     */

    tree_.grow(nullptr, [=](const BoundableEntity* ent, OctreeNode* node) -> bool {
        StaticChunkHolder::ptr static_chunks;

        if(!node->exists("static_chunks")) {
            static_chunks.reset(new StaticChunkHolder());
        } else {
            static_chunks = node->get<StaticChunkHolder::ptr>("static_chunks");
        }

        // If there isn't a chunk for this geom, create one
        if(!static_chunks->chunks.count(geom_id)) {
            static_chunks->chunks.insert(std::make_pair(geom_id, std::make_shared<StaticChunk>(stage.get())));
        }

        //TODO: Add the polygons to the chunk
        auto& static_chunk = static_chunks->chunks.at(geom_id);

        return false;
    });

}

void OctreePartitioner::remove_geom(GeomID geom_id) {

}

void OctreePartitioner::add_actor(ActorID obj) {
    L_DEBUG("Adding actor to the partitioner");

    auto ent = stage->actor(obj);
    for(uint16_t i = 0; i < ent->subactor_count(); ++i) {
        //All subactors are boundable
        BoundableEntity* boundable = &ent->subactor(i);
        tree_.grow(boundable);

        actor_to_registered_subactors_[obj].push_back(boundable);
        boundable_to_renderable_[boundable] = ent->_subactors().at(i);
    }

    //Connect the changed signal
    actor_changed_connections_[obj] = ent->signal_mesh_changed().connect(std::bind(&OctreePartitioner::event_actor_changed, this, std::placeholders::_1));
}

void OctreePartitioner::remove_actor(ActorID obj) {
    L_DEBUG("Removing actor from the partitioner");

    //Remove all boundable subactors that were linked to the actor
    for(BoundableEntity* boundable: actor_to_registered_subactors_[obj]) {
        tree_.shrink(boundable);
        boundable_to_renderable_.erase(boundable);
    }

    //Erase the list of subactors linked to this actor
    actor_to_registered_subactors_.erase(obj);

    //Disconnect the changed signal
    actor_changed_connections_[obj].disconnect();
    actor_changed_connections_.erase(obj);
}

void OctreePartitioner::add_light(LightID obj) {
    //FIXME: THis is nasty and dangerous
    auto light = stage->light(obj);
    BoundableEntity* boundable = light.__object.get();
    assert(boundable);
    tree_.grow(boundable);
    boundable_to_light_[boundable] = obj;
}

void OctreePartitioner::remove_light(LightID obj) {
    auto light = stage->light(obj);
    BoundableEntity* boundable = light.__object.get();
    assert(boundable);
    tree_.shrink(boundable);
    boundable_to_light_.erase(boundable);
}

std::vector<RenderablePtr> OctreePartitioner::geometry_visible_from(CameraID camera_id) {
    std::vector<RenderablePtr> results;

    //If the tree has no root then we return nothing
    if(!tree_.has_root()) {
        return results;
    }

    /**
     *  FIXME: A tree_->objects_visible_from(cam.frustum()); would be faster
     */

    //Go through the visible nodes
    for(OctreeNode* node: tree_.nodes_visible_from(stage->window->camera(camera_id)->frustum())) {
        //Go through the objects
        for(const BoundableEntity* obj: node->objects()) {
            if(container::contains(boundable_to_renderable_, obj)) {
                //Build a list of visible subactors
                results.push_back(boundable_to_renderable_[obj]);
            }
        }
    }

    return results;
}

std::vector<LightID> OctreePartitioner::lights_visible_from(CameraID camera_id) {
    std::vector<LightID> lights;
    return lights;
}

}
