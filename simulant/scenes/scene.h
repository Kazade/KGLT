/* *   Copyright (c) 2011-2017 Luke Benstead https://simulant-engine.appspot.com
 *
 *     This file is part of Simulant.
 *
 *     Simulant is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Simulant is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Simulant.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCENE_H
#define SCENE_H

/**
 *  Allows you to register different scenes of gameplay, and
 *  easily switch between them.
 *
 *  manager->register_scene("/", scene_factory<LoadingScene());
 *  manager->register_scene("/menu", scene_factory<MenuScene());
 *  manager->register_scene("/ingame", scene_factory<GameScene());
 *
 *  manager->activate_scene("/");
 *  manager->load_scene_in_background("/menu");
 *  if(manager->is_loaded("/menu")) {
 *      manager->activate_scene("/menu");
 *  }
 *  manager->unload("/");
 *  manager->activate_scene("/"); // Will cause loading to happen again
 *
 */

#include "../utils/unicode.h"
#include "../types.h"
#include "../window_base.h"
#include "../generic/managed.h"
#include "../generic/property.h"
#include "../interfaces/nameable.h"
#include "../interfaces.h"

namespace smlt {

class SceneLoadException : public std::runtime_error {};

class SceneBase:
    public Nameable,
    public Updateable {
public:
    typedef std::shared_ptr<SceneBase> ptr;

    SceneBase(WindowBase& window, const unicode& name);
    virtual ~SceneBase();

    void load();
    void unload();

    void activate();
    void deactivate();

    bool is_loaded() const { return is_loaded_; }

protected:
    Property<SceneBase, WindowBase> window = { this, &SceneBase::window_ };

    virtual void do_load() = 0;
    virtual void do_unload() {}
    virtual void do_activate() {}
    virtual void do_deactivate() {}

    PipelineID prepare_basic_scene(
        StageID& new_stage,
        CameraID& new_camera,
        AvailablePartitioner partitioner=PARTITIONER_HASH
    );

    WindowBase* window_;

private:
    bool is_loaded_ = false;
};

template<typename T>
class Scene : public SceneBase, public Managed<T> {
public:
    Scene(WindowBase& window, const unicode& name):
        SceneBase(window, name) {}

    void cleanup() override {
        do_unload();
    }
};

}


#endif // SCENE_H