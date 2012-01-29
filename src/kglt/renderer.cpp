#include "glee/GLee.h"

#include "utils/gl_error.h"
#include "kazmath/mat4.h"

#include "scene.h"
#include "renderer.h"
#include "mesh.h"
#include "shader.h"
#include "window.h"

namespace kglt {

void Renderer::start_render(Scene* scene) {
    scene_ = scene;

    if(!options_.texture_enabled) {
        glDisable(GL_TEXTURE_2D);
    } else {
        glEnable(GL_TEXTURE_2D);
    }

    if(options_.wireframe_enabled) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if(!options_.backface_culling_enabled) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }

    glPointSize(options_.point_size);

    kmVec3& pos = scene->camera().position();
    kmQuaternion& rot = scene->camera().rotation();
    kmMat4 rot_mat;
    kmMat4RotationQuaternion(&rot_mat, &rot);

    rot_mat.mat[12] = pos.x;
    rot_mat.mat[13] = pos.y;
    rot_mat.mat[14] = pos.z;

    kmVec3 up;
    kmVec3 forward;
    kmMat4GetForwardVec3(&forward, &rot_mat);
    kmMat4GetUpVec3(&up, &rot_mat);

    gluLookAt(pos.x, pos.y, pos.z,
              pos.x + forward.x, pos.y + forward.y, pos.z + forward.z,
              up.x, up.y, up.z);
              
    float aspect = float(scene->window()->width()) / float(scene->window()->height());
    kmMat4* top = &projection_stack_.top();
    kmMat4PerspectiveProjection(top, 45.0f, aspect, 0.1f, 100.0f);
}

void Renderer::visit(Mesh* mesh) {
    kglt::TextureID tex = mesh->texture(PRIMARY);
    if(tex != NullTextureID) {
        glBindTexture(GL_TEXTURE_2D, scene_->texture(tex).gl_tex());
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    //FIXME: Allow meshes to override the shader
    ShaderProgram& s = scene_->shader(NullShaderID);
    s.bind_attrib(0, "vertex_position");
    s.bind_attrib(1, "vertex_texcoord_1");
    s.set_uniform("texture_1", 0);
    s.activate();

    kmMat4 transform;
    kmMat4Identity(&transform);
    check_and_log_error(__FILE__, __LINE__);
    
    modelview_stack_.push();
        mesh->activate_vbo();
        
        uint32_t stride = (sizeof(float) * 3) + (sizeof(float) * 2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(0));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, BUFFER_OFFSET(sizeof(float) * 3));
        glClientActiveTexture(GL_TEXTURE0);
        
        //FIXME: should be absolute position
        //glTranslatef(mesh->position().x, mesh->position().y, mesh->position().z);
        kmMat4 trans;
        kmMat4Identity(&trans);
        kmMat4Translation(&trans, mesh->position().x, mesh->position().y, mesh->position().z);
        kmMat4Multiply(&modelview_stack_.top(), &modelview_stack_.top(), &trans);
        
        s.set_uniform("modelview_matrix", &modelview_stack_.top());
        s.set_uniform("projection_matrix", &projection_stack_.top());

        if(mesh->arrangement() == MeshArrangement::POINTS) {
            glDrawArrays(GL_POINTS, 0, mesh->vertices().size());        
        } else {
            glDrawArrays(GL_TRIANGLES, 0, mesh->triangles().size());
        }
    modelview_stack_.pop();
}

}
