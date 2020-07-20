#include "simulant/simulant.h"
#include "simulant/shortcuts.h"
#include "simulant/extra.h"

using namespace smlt;

class GameScene : public smlt::Scene<GameScene> {
public:
    GameScene(smlt::Core* core):
        smlt::Scene<GameScene>(core) {}

    void load() {
        stage_ = core->new_stage(smlt::PARTITIONER_FRUSTUM);
        camera_ = stage_->new_camera();
        pipeline_ = compositor->render(stage_, camera_);

        pipeline_->set_clear_flags(BUFFER_CLEAR_ALL);
        pipeline_->viewport->set_colour(smlt::Colour::GREY);
        link_pipeline(pipeline_);

        core->vfs->add_search_path("sample_data/quake2/textures");

        auto mesh = stage_->assets->new_mesh_from_file("sample_data/quake2/maps/demo1.bsp");
        stage_->new_geom_with_mesh(mesh->id());

        yield_coroutine();

        auto entities = mesh->data->get<smlt::Q2EntityList>("entities");

        std::for_each(entities.begin(), entities.end(), [&](Q2Entity& ent) {
            if(ent["classname"] == "info_player_start") {
                auto position = ent["origin"];
                std::vector<unicode> coords = _u(position).split(" ");

                //Needed because the Quake 2 coord system is weird
                Mat4 rotation_x = Mat4::as_rotation_x(Degrees(-90));
                Mat4 rotation_y = Mat4::as_rotation_y(Degrees(90.0f));
                Mat4 rotation = rotation_y * rotation_x;

                smlt::Vec3 pos(
                    coords[0].to_float(),
                    coords[1].to_float(),
                    coords[2].to_float()
                );

                pos = pos.rotated_by(rotation);
                camera_->move_to_absolute(pos);
            }

            yield_coroutine();
        });

        // Add a fly controller to the camera for user input
        camera_->new_behaviour<behaviours::Fly>(core);

        camera_->set_perspective_projection(
            Degrees(45.0),
            float(core->width()) / float(core->height()),
            1.0,
            1000.0
        );

        stage_->new_light_as_directional();
        yield_coroutine();
    }

private:
    StagePtr stage_;
    CameraPtr camera_;
    PipelinePtr pipeline_;
};


class Q2Sample: public smlt::Application {
public:
    Q2Sample(const smlt::AppConfig& config):
        smlt::Application(config) {}

private:
    bool init() {
        scenes->register_scene<GameScene>("main");
        scenes->load_in_background("main", true); //Do loading in a background thread, but show immediately when done
        scenes->activate("_loading"); // Show the loading screen in the meantime
        return true;
    }
};


int main(int argc, char* argv[]) {
    smlt::AppConfig config;
    config.title = "Quake 2 Mesh Loader";
    config.fullscreen = false;
    config.width = 1280;
    config.height = 960;

    Q2Sample app(config);
    return app.run();
}
