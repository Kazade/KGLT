#pragma once

#include "scene.h"
#include "../controllers/rigid_body.h"

namespace smlt {

template<typename T>
class PhysicsScene:
    public Scene<T> {

protected:
    PhysicsScene(WindowBase& window):
        Scene<T>(window) {}

    Property<PhysicsScene, smlt::controllers::RigidBodySimulation> physics = { this, &PhysicsScene::physics_ };

    virtual void _fixed_update_thunk(double step) {
        Scene<T>::_fixed_update_thunk(step);
        if(physics_) {
            physics_->fixed_update(step);
        }
    }

private:
    void pre_load() override {
        physics_.reset(new smlt::controllers::RigidBodySimulation());
    }

    void post_unload() override {
        physics_.reset();
    }

    std::shared_ptr<smlt::controllers::RigidBodySimulation> physics_;
};

}
