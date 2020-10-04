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

#ifndef MATERIAL_SCRIPT_H
#define MATERIAL_SCRIPT_H

#include <stdexcept>
#include <string>
#include <vector>

#include "../generic/managed.h"
#include "../material.h"
#include "../types.h"
#include "../loader.h"

namespace smlt {

class SyntaxError : public std::logic_error {
public:
    SyntaxError(const unicode& what):
        std::logic_error(what.encode()) {}
};

class MaterialScript :
    public RefCounted<MaterialScript> {
public:
    MaterialScript(StreamPtr data, const unicode &filename);
    void generate(Material& material);

private:
    unicode filename_;
    StreamPtr data_;
};

namespace loaders {


class MaterialScriptLoader:
    public Loader {

public:
    MaterialScriptLoader(const unicode& filename, StreamPtr data):
        Loader(filename, data) {
        parser_ = MaterialScript::create(data, filename);
    }

    void into(Loadable& resource, const LoaderOptions& options) override;

private:
    MaterialScript::ptr parser_;
};

class MaterialScriptLoaderType : public LoaderType {
public:
    unicode name() { return "material"; }
    bool supports(const unicode& filename) const {
        return filename.lower().contains(".smat");
    }

    Loader::ptr loader_for(const unicode& filename, StreamPtr data) const {
        return Loader::ptr(new MaterialScriptLoader(filename, data));
    }
};

}
}

#endif // MATERIAL_SCRIPT_H
