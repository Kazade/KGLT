#ifndef KGLT_TGA_LOADER_H
#define KGLT_TGA_LOADER_H

#include "../loader.h"

namespace kglt {
namespace loaders {

class TextureLoader : public Loader {
public:
    TextureLoader(const unicode& filename):
        Loader(filename) {}

    void into(Loadable& resource, const LoaderOptions& options = LoaderOptions());
};

class TextureLoaderType : public LoaderType {
public:
    TextureLoaderType() {

    }

    ~TextureLoaderType() {}

    unicode name() { return "texture_loader"; }
    bool supports(const unicode& filename) const override {
        return filename.lower().contains(".tga") || filename.lower().contains(".png");
    }

    Loader::ptr loader_for(const unicode& filename) const {
        return Loader::ptr(new TextureLoader(filename));
    }
};

}
}

#endif
