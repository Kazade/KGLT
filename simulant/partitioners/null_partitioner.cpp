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

#include "../stage.h"
#include "../nodes/camera.h"
#include "../nodes/actor.h"
#include "../nodes/light.h"
#include "../nodes/particle_system.h"
#include "../nodes/geom.h"

#include "null_partitioner.h"

namespace smlt {

void NullPartitioner::lights_and_geometry_visible_from(
        CameraID camera_id, std::vector<LightID> &lights_out,
        std::vector<StageNode*> &geom_out) {

    _S_UNUSED(camera_id);

    for(auto& key: all_nodes_) {
        if(key.first == typeid(Light)) {
            lights_out.push_back(make_unique_id_from_key<LightID>(key));
        } else if(key.first == typeid(Actor)) {
            auto actor = stage->actor(make_unique_id_from_key<ActorID>(key));
            geom_out.push_back(actor);
        } else if(key.first == typeid(Geom)) {
            auto geom = stage->geom(make_unique_id_from_key<GeomID>(key));
            geom_out.push_back(geom);
        } else if(key.first == typeid(ParticleSystem)) {
            auto ps = stage->particle_system(make_unique_id_from_key<ParticleSystemID>(key));
            geom_out.push_back(ps);
        } else {
            assert(0 && "Not implemented");
        }
    }
}

void NullPartitioner::apply_staged_write(const UniqueIDKey& key, const StagedWrite &write) {
    if(write.operation == WRITE_OPERATION_ADD) {
        all_nodes_.insert(key);
    } else if(write.operation == WRITE_OPERATION_REMOVE) {
        all_nodes_.erase(key);
    } else if(write.operation == WRITE_OPERATION_UPDATE) {
        // Do nothing!
    }
}



}
