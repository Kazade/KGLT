#ifndef TEST_SOUND_H
#define TEST_SOUND_H

#include <cstdlib>
#include "simulant/simulant.h"
#include "simulant/test.h"


class SoundTest : public smlt::test::SimulantTestCase {
public:
    void set_up() {
#ifdef __APPLE__
    bool skip = bool(std::getenv("TRAVIS"));
    skip_if(skip, "OSX Travis builds hang on sound tests :(");
#endif

        SimulantTestCase::set_up();

        stage_ = core->new_stage();
        camera_ = stage_->new_camera();
    }

    void tear_down() {
        SimulantTestCase::tear_down();
        core->destroy_stage(stage_->id());
    }

    void test_audio_listener() {
        assert_false(core->has_explicit_audio_listener());
        assert_is_null(core->audio_listener());

        auto p = core->compositor->render(stage_, camera_);
        p->activate();

        // Make the first camera of the first pipeline the audio listener
        assert_equal(core->audio_listener(), camera_);
        assert_false(core->has_explicit_audio_listener());

        auto actor = stage_->new_actor();
        core->set_audio_listener(actor);

        assert_equal(core->audio_listener(), actor);
        assert_true(core->has_explicit_audio_listener());

        stage_->destroy_actor(actor);
        core->run_frame(); // actually destroy

        assert_equal(core->audio_listener(), camera_);
        assert_false(core->has_explicit_audio_listener());
    }

    void test_2d_sound_output() {
        smlt::SoundID sound = core->shared_assets->new_sound_from_file("test_sound.ogg");

        assert_false(core->playing_sound_count());

        core->play_sound(sound);

        assert_true(core->playing_sound_count());

        while(core->playing_sound_count()) {
            core->run_frame();
        }
    }

    void test_3d_sound_output() {
        smlt::SoundID sound = stage_->assets->new_sound_from_file("test_sound.ogg");

        auto actor = stage_->new_actor();
        actor->move_to(10, 0, 0);

        assert_false(actor->playing_sound_count());

        actor->play_sound(sound);

        assert_true(actor->playing_sound_count());

        // Finish playing the sound
        while(core->playing_sound_count()) {
            core->run_frame();
        }
    }

private:
    smlt::CameraPtr camera_;
    smlt::StagePtr stage_;

};
#endif // TEST_SOUND_H
