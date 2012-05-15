#ifndef KGLT_GENERIC_RENDERER_H_INCLUDED
#define KGLT_GENERIC_RENDERER_H_INCLUDED

#include <iostream>

#include "kglt/renderer.h"

namespace kglt {

class Mesh;
class Camera;
class Scene;

class GenericRenderer : public Renderer {
public:
	typedef std::tr1::shared_ptr<Renderer> ptr;

	GenericRenderer() {}
	GenericRenderer(const RenderOptions& options):
        Renderer(options) {}
            
    void visit(Mesh* mesh);

    //The default camera is set in start_render
    //TODO: Add debug mode which renders camera positions
    void visit(Camera* camera) {}

    //We don't really care about the Scene object, but it is an Object
    //so this method must exist
    void visit(Scene* scene) {}

    //Sprites are just containers
    void visit(Sprite* sprite) {}	  
    
private:
	void on_start_render();
};

}

#endif // RENDERER_H_INCLUDED
