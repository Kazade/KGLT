#include <GLee.h>

#include "generic_renderer.h"

#include "../shader.h"
#include "../scene.h"
#include "../camera.h"
#include "../light.h"
#include "../partitioner.h"

#include "kazmath/mat4.h"
#include "../utils/gl_error.h"

namespace kglt {

/*
 * FIXME: Stupid argument ordering
 */
void GenericRenderer::set_auto_uniforms_on_shader(
    ShaderProgram& s,
    Scene& scene,
    const std::vector<LightID>& lights_within_range,
    uint32_t iteration,
    CameraID camera,
    SubEntity& subentity,
    MaterialPass& pass) {

    //Calculate the modelview-projection matrix
    kmMat4 modelview_projection;
    kmMat4 modelview;

    kglt::Camera& cam = current_subscene().camera(camera);

    const kmMat4 model = subentity._parent().absolute_transformation();
    const kmMat4& view = cam.view_matrix();
    const kmMat4& projection = cam.projection_matrix();

    kmMat4Multiply(&modelview, &view, &model);
    kmMat4Multiply(&modelview_projection, &projection, &modelview);

    if(s.params().uses_auto(SP_AUTO_MODELVIEW_PROJECTION_MATRIX)) {
        s.params().set_mat4x4(
            s.params().auto_uniform_variable_name(SP_AUTO_MODELVIEW_PROJECTION_MATRIX),
            modelview_projection
        );
    }

    if(s.params().uses_auto(SP_AUTO_MODELVIEW_MATRIX)) {
        s.params().set_mat4x4(
            s.params().auto_uniform_variable_name(SP_AUTO_MODELVIEW_MATRIX),
            modelview
        );
    }

    if(s.params().uses_auto(SP_AUTO_PROJECTION_MATRIX)) {
        s.params().set_mat4x4(
            s.params().auto_uniform_variable_name(SP_AUTO_PROJECTION_MATRIX),
            projection
        );
    }

    if(s.params().uses_auto(SP_AUTO_INVERSE_TRANSPOSE_MODELVIEW_PROJECTION_MATRIX)) {
        kmMat3 inverse_transpose_modelview_proj;

        kmMat3AssignMat4(&inverse_transpose_modelview_proj, &modelview_projection);
        kmMat3Inverse(&inverse_transpose_modelview_proj, &inverse_transpose_modelview_proj);
        kmMat3Transpose(&inverse_transpose_modelview_proj, &inverse_transpose_modelview_proj);

        s.params().set_mat3x3(
            s.params().auto_uniform_variable_name(SP_AUTO_INVERSE_TRANSPOSE_MODELVIEW_PROJECTION_MATRIX),
            inverse_transpose_modelview_proj
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_POSITION)) {
        if(iteration < lights_within_range.size()) {
            //Transform the light position by the modelview matrix before
            //passing to the shader
            kglt::Light& light = current_subscene().light(lights_within_range.at(iteration));

            Vec4 light_pos = Vec4(light.absolute_position(), (light.type() == LIGHT_TYPE_DIRECTIONAL) ? 0.0 : 1.0);

            kmVec4MultiplyMat4(&light_pos, &light_pos, &view);

            s.params().set_vec4(
                s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_POSITION),
                light_pos
            );
        }
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_AMBIENT)) {
        kglt::Colour ambient(0, 0, 0, 1);
        if(iteration < lights_within_range.size()) {
            ambient = current_subscene().light(lights_within_range.at(iteration)).ambient();
        }
        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_AMBIENT),
            ambient
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_DIFFUSE)) {
        kglt::Colour diffuse(0, 0, 0, 1);

        if(iteration < lights_within_range.size()) {
            diffuse = current_subscene().light(lights_within_range.at(iteration)).diffuse();
        }

        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_DIFFUSE),
            diffuse
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_SPECULAR)) {
        kglt::Colour specular(0, 0, 0, 1);

        if(iteration < lights_within_range.size()) {
            specular = current_subscene().light(lights_within_range.at(iteration)).specular();
        }

        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_SPECULAR),
            specular
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_CONSTANT_ATTENUATION)) {
        float constant_attenuation = 1.0;

        if(iteration < lights_within_range.size()) {
            constant_attenuation = current_subscene().light(lights_within_range.at(iteration)).constant_attenuation();
        }

        s.params().set_float(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_CONSTANT_ATTENUATION),
            constant_attenuation
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_LINEAR_ATTENUATION)) {
        float linear_attenuation = 1.0;

        if(iteration < lights_within_range.size()) {
            linear_attenuation = current_subscene().light(lights_within_range.at(iteration)).linear_attenuation();
        }

        s.params().set_float(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_LINEAR_ATTENUATION),
            linear_attenuation
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_QUADRATIC_ATTENUATION)) {
        float quadratic_attenuation = 1.0;

        if(iteration < lights_within_range.size()) {
            quadratic_attenuation = current_subscene().light(lights_within_range.at(iteration)).quadratic_attenuation();
        }

        s.params().set_float(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_QUADRATIC_ATTENUATION),
            quadratic_attenuation
        );
    }

    if(s.params().uses_auto(SP_AUTO_LIGHT_GLOBAL_AMBIENT)) {
        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_LIGHT_GLOBAL_AMBIENT),
            current_subscene().ambient_light()
        );
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_AMBIENT)) {
        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_AMBIENT),
            pass.ambient()
        );
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_DIFFUSE)) {
        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_DIFFUSE),
            pass.diffuse()
        );
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_SPECULAR)) {
        s.params().set_colour(
            s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_SPECULAR),
            pass.specular()
        );
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_SHININESS)) {
        s.params().set_float(
            s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_SHININESS),
            pass.shininess()
        );
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_TEX_MATRIX0)) {
        //Pass as an array to the shader
        if(pass.texture_unit_count()) {
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX0),
                pass.texture_unit(0).matrix()
            );
        } else {
            kmMat4 identity;
            kmMat4Identity(&identity);
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX0),
                identity
            );
        }
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_TEX_MATRIX1)) {
        //Pass as an array to the shader
        if(pass.texture_unit_count() > 1) {
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX1),
                pass.texture_unit(1).matrix()
            );
        } else {
            kmMat4 identity;
            kmMat4Identity(&identity);
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX1),
                identity
            );
        }
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_TEX_MATRIX2)) {
        //Pass as an array to the shader
        if(pass.texture_unit_count() > 2) {
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX2),
                pass.texture_unit(2).matrix()
            );
        } else {
            kmMat4 identity;
            kmMat4Identity(&identity);
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX1),
                identity
            );
        }
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_TEX_MATRIX3)) {
        //Pass as an array to the shader
        if(pass.texture_unit_count() > 3) {
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX3),
                pass.texture_unit(3).matrix()
            );
        } else {
            kmMat4 identity;
            kmMat4Identity(&identity);
            s.params().set_mat4x4(
                s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_TEX_MATRIX1),
                identity
            );
        }
    }

    if(s.params().uses_auto(SP_AUTO_MATERIAL_ACTIVE_TEXTURE_UNITS)) {
        s.params().set_int(
            s.params().auto_uniform_variable_name(SP_AUTO_MATERIAL_ACTIVE_TEXTURE_UNITS),
            pass.texture_unit_count()
        );
    }
}

template<typename EnabledMethod, typename OffsetMethod>
void send_attribute(ShaderProgram& s,
                    ShaderAvailableAttributes attr,
                    const VertexData& data,
                    EnabledMethod exists_on_data_predicate,
                    OffsetMethod offset_func) {

    if(!s.params().uses_attribute(attr)) {
        return;
    }

    int32_t loc = s.get_attrib_loc(s.params().attribute_variable_name(attr));
    if(loc < 0) {
        L_WARN("Couldn't locate attribute, on the shader");
        return;
    }

    auto get_has_attribute = sigc::mem_fun(data, exists_on_data_predicate);
    auto get_offset = sigc::mem_fun(data, offset_func);
    if(get_has_attribute()) {
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(
            loc,
            SHADER_ATTRIBUTE_SIZES.find(attr)->second,
            GL_FLOAT,
            GL_FALSE,
            data.stride(),
            BUFFER_OFFSET(get_offset())
        );
    } else {
        L_WARN("Couldn't locate attribute on the mesh: " + boost::lexical_cast<std::string>(attr));
    }
}

void GenericRenderer::set_auto_attributes_on_shader(ShaderProgram& s, SubEntity& buffer) {
    /*
     *  Binding attributes generically is hard. So we have some template magic in the send_attribute
     *  function above that takes the VertexData member functions we need to provide the attribute
     *  and just makes the whole thing generic. Before this was 100s of lines of boilerplate. Thank god
     *  for templates!
     */
    send_attribute(s, SP_ATTR_VERTEX_POSITION, buffer.vertex_data(), &VertexData::has_positions, &VertexData::position_offset);
    send_attribute(s, SP_ATTR_VERTEX_TEXCOORD0, buffer.vertex_data(), &VertexData::has_texcoord0, &VertexData::texcoord0_offset);
    send_attribute(s, SP_ATTR_VERTEX_TEXCOORD1, buffer.vertex_data(), &VertexData::has_texcoord1, &VertexData::texcoord1_offset);
    send_attribute(s, SP_ATTR_VERTEX_TEXCOORD2, buffer.vertex_data(), &VertexData::has_texcoord2, &VertexData::texcoord2_offset);
    send_attribute(s, SP_ATTR_VERTEX_TEXCOORD3, buffer.vertex_data(), &VertexData::has_texcoord3, &VertexData::texcoord3_offset);
    send_attribute(s, SP_ATTR_VERTEX_DIFFUSE, buffer.vertex_data(), &VertexData::has_diffuse, &VertexData::diffuse_offset);
    send_attribute(s, SP_ATTR_VERTEX_NORMAL, buffer.vertex_data(), &VertexData::has_normals, &VertexData::normal_offset);
}

void GenericRenderer::set_blending_mode(BlendType type) {
    if(type == BLEND_NONE) {
        glDisable(GL_BLEND);
        return;
    }

    glEnable(GL_BLEND);
    switch(type) {
        case BLEND_ADD: glBlendFunc(GL_ONE, GL_ONE);
        break;
        case BLEND_ALPHA: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
        case BLEND_COLOUR: glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        break;
        case BLEND_MODULATE: glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
        case BLEND_ONE_ONE_MINUS_ALPHA: glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    default:
        throw ValueError("Invalid blend type specified");
    }
}

void GenericRenderer::render_subentity(SubEntity& buffer, CameraID camera) {
    if(!buffer.index_data().count()) {
        return;
    }

    buffer.vertex_data().buffer_object().bind();
    buffer.index_data().buffer_object().bind();

    check_and_log_error(__FILE__, __LINE__);

    MaterialID mid = buffer.material_id();
    if(!mid) {
        mid = scene().default_material_id();
    }
    Material& mat = current_subscene().material(mid);
    MaterialTechnique& technique = mat.technique(DEFAULT_MATERIAL_SCHEME);

    for(uint32_t i = 0; i < technique.pass_count(); ++i) {
        check_and_log_error(__FILE__, __LINE__);

        MaterialPass& pass = technique.pass(i);

        glPointSize(pass.point_size());
        glLineWidth(pass.line_width());

        set_blending_mode(pass.blending());

        //Grab and activate the shader for the pass
        if(!pass.shader()) {
            L_ERROR("The current pass has no attached shader, so nothing will be rendered");
            continue;
        }

        //FIXME: lights within range of what?
        Vec3 pos;
        std::vector<LightID> lights = current_subscene().partitioner().lights_within_range(pos);
        uint32_t iteration_count = 1;
        if(pass.iteration() == ITERATE_N) {
            iteration_count = pass.max_iterations();
        } else if (pass.iteration() == ITERATE_ONCE_PER_LIGHT) {
            iteration_count = std::min<uint32_t>(lights.size(), pass.max_iterations());
        }

        //Attributes don't change per-iteration of a pass
        set_auto_attributes_on_shader(s, buffer);

        for(uint32_t j = 0; j < iteration_count; ++j) {
            set_auto_uniforms_on_shader(s, scene(), lights, j, camera, buffer, pass); //Uniforms might change depending on the iteration

            //Render the mesh, once for each iteration of the pass
            if(buffer.arrangement() == MESH_ARRANGEMENT_POINTS) {
                glDrawElements(GL_POINTS, buffer.index_data().count(), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
            } else if(buffer.arrangement() == MESH_ARRANGEMENT_LINES) {
                glDrawElements(GL_LINES, buffer.index_data().count(), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
            } else if(buffer.arrangement() == MESH_ARRANGEMENT_LINE_STRIP) {
                glDrawElements(GL_LINE_STRIP, buffer.index_data().count(), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
            } else if(buffer.arrangement() == MESH_ARRANGEMENT_TRIANGLES) {
                glDrawElements(GL_TRIANGLES, buffer.index_data().count(), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
            } else if(buffer.arrangement() == MESH_ARRANGEMENT_TRIANGLE_STRIP)  {
                glDrawElements(GL_TRIANGLE_STRIP, buffer.index_data().count(), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
            } else if(buffer.arrangement() == MESH_ARRANGEMENT_TRIANGLE_FAN)  {
                glDrawElements(GL_TRIANGLE_FAN, buffer.index_data().count(), GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
            }
            else {
                assert(0);
            }
        }

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
        glDisableVertexAttribArray(4);
        glDisableVertexAttribArray(5);
        glDisableVertexAttribArray(6);

        check_and_log_error(__FILE__, __LINE__);
    }
}

}
