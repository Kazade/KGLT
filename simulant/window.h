/* *   Copyright (c) 2011-2017 Luke Benstead https://simulant-engine.appspot.com
 *
 *     This file is part of Simulant.
 *
 *     Simulant is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Simulant is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU Lesser General Public License for more details.
 *
 *     You should have received a copy of the GNU Lesser General Public License
 *     along with Simulant.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIMULANT_WINDOW_BASE_H
#define SIMULANT_WINDOW_BASE_H

#include <memory>

#include "logging.h"

#include "generic/property.h"
#include "generic/object_manager.h"
#include "generic/data_carrier.h"

#include "vfs.h"
#include "idle_task_manager.h"
#include "input/input_state.h"
#include "types.h"
#include "sound.h"
#include "stage_manager.h"
#include "scenes/scene_manager.h"
#include "loader.h"
#include "event_listener.h"
#include "time_keeper.h"
#include "stats_recorder.h"
#include "screen.h"
#include "coroutines/coroutine.h"

namespace smlt {

class AssetManager;
class InputManager;

namespace ui {
    class Interface;
}

namespace scenes {
    class Loading;
}

class Application;
class InputState;

class Loader;
class LoaderType;
class Compositor;
class SceneImpl;
class VirtualGamepad;
class Renderer;
class Panel;

typedef std::shared_ptr<Loader> LoaderPtr;
typedef std::shared_ptr<LoaderType> LoaderTypePtr;


typedef sig::signal<void ()> FrameStartedSignal;
typedef sig::signal<void ()> FrameFinishedSignal;
typedef sig::signal<void ()> PreSwapSignal;
typedef sig::signal<void ()> PostIdleSignal;

typedef sig::signal<void (float)> FixedUpdateSignal;
typedef sig::signal<void (float)> UpdateSignal;
typedef sig::signal<void (float)> LateUpdateSignal;

typedef sig::signal<void ()> ShutdownSignal;

typedef sig::signal<void (std::string, Screen*)> ScreenAddedSignal;
typedef sig::signal<void (std::string, Screen*)> ScreenRemovedSignal;

class Window :
    public StageManager,
    public Loadable,
    public RenderTarget,
    public EventListenerManager {

    DEFINE_SIGNAL(FrameStartedSignal, signal_frame_started);
    DEFINE_SIGNAL(FrameFinishedSignal, signal_frame_finished);
    DEFINE_SIGNAL(PreSwapSignal, signal_pre_swap);
    DEFINE_SIGNAL(PostIdleSignal, signal_post_idle);
    DEFINE_SIGNAL(FixedUpdateSignal, signal_fixed_update);
    DEFINE_SIGNAL(UpdateSignal, signal_update);
    DEFINE_SIGNAL(LateUpdateSignal, signal_late_update);
    DEFINE_SIGNAL(ShutdownSignal, signal_shutdown);

    DEFINE_SIGNAL(ScreenAddedSignal, signal_screen_added);
    DEFINE_SIGNAL(ScreenRemovedSignal, signal_screen_removed);

    friend class Screen;  /* Screen needs to call render_screen */
public:
    typedef std::shared_ptr<Window> ptr;
    static const int STEPS_PER_SECOND = 60;

    template<typename T>
    static std::shared_ptr<Window> create(Application* app) {
        auto window = std::make_shared<T>();
        window->set_application(app);
        return window;
    }

    virtual ~Window();

    virtual bool create_window(
        uint16_t width,
        uint16_t height,
        uint8_t bpp,
        bool fullscreen,
        bool enable_vsync
    );

    LoaderPtr loader_for(const Path &filename, LoaderHint hint=LOADER_HINT_NONE);
    LoaderPtr loader_for(const std::string& loader_name, const Path& filename);
    LoaderTypePtr loader_type(const std::string& loader_name) const;

    void register_loader(LoaderTypePtr loader_type);

    virtual void set_title(const std::string& title) = 0;
    virtual void cursor_position(int32_t& mouse_x, int32_t& mouse_y) = 0;
    virtual void show_cursor(bool cursor_shown=true) = 0;
    virtual void lock_cursor(bool cursor_locked=true) = 0;

    virtual void check_events() = 0;
    virtual void swap_buffers() = 0;

    bool is_paused() const { return is_paused_; }

    uint16_t width() const override { return width_; }
    uint16_t height() const override { return height_; }
    bool is_fullscreen() const { return fullscreen_; }
    bool vsync_enabled() const { return vsync_enabled_; }

    float aspect_ratio() const;

    bool run_frame();

    void set_logging_level(LogLevel level);

    void stop_running() { is_running_ = false; }
    bool is_shutting_down() const { return is_running_ == false; }

    void enable_virtual_joypad(VirtualGamepadConfig config, bool flipped=false);
    void disable_virtual_joypad();
    bool has_virtual_joypad() const { return bool(virtual_gamepad_); }

    void reset();

    Vec2 coordinate_from_normalized(Ratio rx, Ratio ry) {
        return Vec2(
            uint16_t(float(width()) * rx),
            uint16_t(float(height()) * ry)
        );
    }

    void on_finger_down(
        TouchPointID touch_id,
        float normalized_x, float normalized_y, float pressure=1.0
    );

    void on_finger_up(
        TouchPointID touch_id,
        float normalized_x, float normalized_y
    );

    void on_finger_motion(
        TouchPointID touch_id,
        float normalized_x, float normalized_y,
        float dx, float dy // Between -1.0 and +1.0
    );

    void on_key_down(KeyboardCode code, ModifierKeyState modifiers);
    void on_key_up(KeyboardCode code, ModifierKeyState modifiers);

    /* Return the number of screens connected */
    std::size_t screen_count() const;

    /* Return a specific screen given its name */
    Screen* screen(const std::string& name) const;

    void each_screen(std::function<void (std::string, Screen*)> callback);

    /* Private API for Window subclasses (public for testing)
       don't call this directly
    */
    Screen* _create_screen(
        const std::string& name,
        uint16_t width,
        uint16_t height,
        ScreenFormat format,
        uint16_t refresh_rate);

    void _destroy_screen(const std::string& name);

    void _fixed_update_thunk(float dt) override;
    void _update_thunk(float dt) override;

    /* Creates the window, but doesn't do any context initialisation */
    virtual bool _init_window() = 0;

    /* Initialises any renderer context */
    virtual bool _init_renderer(Renderer* renderer) = 0;

    bool initialize_assets_and_devices();
    void _clean_up();

    /* Audio listener stuff */

    /* Returns the current audio listener, or NULL if there
     * is no explicit audio listener set, and there are no current
     * render pipelines.
     *
     * Behaviour is:
     *
     *  - Explictly set listener
     *  - Or, First camera of first render pipeline
     *  - Or, NULL
     */
    StageNode* audio_listener();

    /* Sets a stage node explicitly as the audio listener */
    void set_audio_listener(StageNode* node);

    /* Returns true if an explicit audio listener is being used */
    bool has_explicit_audio_listener() const;


    /* Coroutines */
    void start_coroutine(std::function<void ()> func);

    void update_idle_tasks_and_coroutines();

private:
    std::list<cort::CoroutineID> coroutines_;
    void update_coroutines();
    void stop_all_coroutines();

protected:
    std::shared_ptr<Renderer> renderer_;

    void set_vsync_enabled(bool vsync) {
        vsync_enabled_ = vsync;
    }

    void set_width(uint16_t width) {
        width_ = width;
    }

    void set_height(uint16_t height) {
        height_ = height;
    }

    void set_bpp(uint16_t bpp) {
        bpp_ = bpp;
    }

    void set_fullscreen(bool val) {
        fullscreen_ = val;
    }

    virtual void destroy_window() = 0;

    Window();

    void set_paused(bool value=true);
    void set_has_context(bool value=true);

    bool has_context() const { return has_context_; }
    thread::Mutex& context_lock() { return context_lock_; }

    void set_application(Application* app) { application_ = app; }

    void set_escape_to_quit(bool value=true) { escape_to_quit_ = value; }
    bool escape_to_quit_enabled() const { return escape_to_quit_; }
public:
    // Panels
    void register_panel(uint8_t function_key, std::shared_ptr<Panel> panel);
    void unregister_panel(uint8_t function_key);
    void toggle_panel(uint8_t id);
    void activate_panel(uint8_t id);
    void deactivate_panel(uint8_t id);
    bool panel_is_active(uint8_t id);
private:
    Application* application_ = nullptr;

    void create_defaults();
    virtual void initialize_input_controller(InputState& controller) = 0;

    bool can_attach_sound_by_id() const { return false; }

    std::shared_ptr<SharedAssetManager> asset_manager_;

    bool initialized_;

    uint16_t width_ = 0;
    uint16_t height_ = 0;
    uint16_t bpp_ = 0;
    bool fullscreen_ = false;
    bool vsync_enabled_ = false;

    bool escape_to_quit_ = true;

    std::vector<LoaderTypePtr> loaders_;
    bool is_running_;

    IdleTaskManager idle_;

    bool is_paused_ = false;
    bool has_context_ = false;


    struct PanelEntry {
        std::shared_ptr<Panel> panel;
    };

    std::unordered_map<uint8_t, PanelEntry> panels_;

    /*
     *  Sometimes we need to destroy or recreate the GL context, if that happens while we are rendering in the
     *  main thread, then bad things happen. This lock exists so that we don't destroy the context while we are rendering.
     *  We obtain the lock before rendering, and release it after. Likewise we obtain the lock while destroying the context
     *  (we can use has_context to make sure we don't start rendering when there is no context) */
    thread::Mutex context_lock_;

    void destroy() {}

    VirtualFileSystem::ptr vfs_;

    float frame_counter_time_;
    int32_t frame_counter_frames_;
    float frame_time_in_milliseconds_;

    std::shared_ptr<scenes::Loading> loading_;
    std::shared_ptr<smlt::Compositor> compositor_;
    generic::DataCarrier data_carrier_;
    std::shared_ptr<VirtualGamepad> virtual_gamepad_;
    std::shared_ptr<TimeKeeper> time_keeper_;

    StatsRecorder stats_;

    std::shared_ptr<SoundDriver> sound_driver_;

    virtual std::shared_ptr<SoundDriver> create_sound_driver(const std::string& from_config) = 0;

    std::shared_ptr<InputState> input_state_;
    std::shared_ptr<InputManager> input_manager_;

    void await_frame_time();
    uint64_t last_frame_time_us_ = 0;
    float requested_frame_time_ms_ = 0;

    std::unordered_map<std::string, Screen::ptr> screens_;

    /* This is called by Screens to render themselves to devices. Default behaviour is a no-op */
    virtual void render_screen(Screen* screen, const uint8_t* data) {
        _S_UNUSED(screen);
        _S_UNUSED(data);
    }

    /* To be overridden by subclasses if external screens need some kind of initialization/clean_up */
    virtual bool initialize_screen(Screen* screen) {
        _S_UNUSED(screen);
        return true;
    }

    virtual void shutdown_screen(Screen* screen) {
        _S_UNUSED(screen);
    }

    StageNode* audio_listener_ = nullptr;
protected:
    InputState* _input_state() const { return input_state_.get(); }
public:
    //Read only properties
    S_DEFINE_PROPERTY(shared_assets, &Window::asset_manager_);
    S_DEFINE_PROPERTY(application, &Window::application_);
    S_DEFINE_PROPERTY(virtual_joypad, &Window::virtual_gamepad_);
    S_DEFINE_PROPERTY(renderer, &Window::renderer_);
    S_DEFINE_PROPERTY(time_keeper, &Window::time_keeper_);
    S_DEFINE_PROPERTY(idle, &Window::idle_);
    S_DEFINE_PROPERTY(data, &Window::data_carrier_);
    S_DEFINE_PROPERTY(vfs, &Window::vfs_);
    S_DEFINE_PROPERTY(input, &Window::input_manager_);
    S_DEFINE_PROPERTY(input_state, &Window::input_state_);
    S_DEFINE_PROPERTY(stats, &Window::stats_);
    S_DEFINE_PROPERTY(compositor, &Window::compositor_);

    SoundDriver* _sound_driver() const { return sound_driver_.get(); }

    void run_update();
    void run_fixed_updates();
    void request_frame_time(float ms);
};

}

#endif
