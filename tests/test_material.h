#ifndef TEST_MATERIAL_H
#define TEST_MATERIAL_H

#include "simulant/simulant.h"
#include "simulant/test.h"


class MaterialTest : public smlt::test::SimulantTestCase {
public:
    void test_material_initialization() {
        auto mat = window->shared_assets->material(window->shared_assets->new_material());

        mat->set_pass_count(1);

        this->assert_equal((uint32_t)1, mat->pass_count()); //Should return the default pass
        this->assert_true(smlt::Colour::WHITE == mat->pass(0)->diffuse()); //this->assert_true the default pass sets white as the default
        this->assert_true(smlt::Colour::WHITE == mat->pass(0)->ambient()); //this->assert_true the default pass sets white as the default
        this->assert_true(smlt::Colour::WHITE == mat->pass(0)->specular()); //this->assert_true the default pass sets white as the default
        this->assert_equal(0.0, mat->pass(0)->shininess());
    }

    void test_material_applies_to_mesh() {
        smlt::MaterialID mid = window->shared_assets->new_material();
        smlt::MeshID mesh_id = window->shared_assets->new_mesh(smlt::VertexSpecification::POSITION_ONLY);
        auto mesh = window->shared_assets->mesh(mesh_id);
        smlt::SubMesh* sm = mesh->new_submesh_with_material("test", mid);
        this->assert_equal(mid, sm->material_id());
    }

    void test_property_heirarchy() {
        auto mat = window->shared_assets->new_material().fetch();

        mat->set_diffuse(smlt::Colour::RED);
        mat->set_pass_count(2);

        auto pass1 = mat->pass(0);
        auto pass2 = mat->pass(1);

        assert_equal(pass1->diffuse(), smlt::Colour::RED);
        assert_equal(pass2->diffuse(), smlt::Colour::RED);

        pass1->set_diffuse(smlt::Colour::GREEN);

        assert_equal(pass1->diffuse(), smlt::Colour::GREEN);
        assert_equal(pass2->diffuse(), smlt::Colour::RED);
    }

    void test_texture_unit() {
        auto mat = window->shared_assets->new_material().fetch();
        auto tex = window->shared_assets->new_texture();

        mat->set_diffuse_map(tex);
        mat->set_pass_count(2);

        auto pass1 = mat->pass(0);
        auto pass2 = mat->pass(1);

        assert_equal(pass1->diffuse_map().texture_id, tex);
        assert_equal(pass2->diffuse_map().texture_id, tex);

        auto tex2 = window->shared_assets->new_texture();

        pass1->set_diffuse_map(tex2);

        assert_equal(pass1->diffuse_map().texture_id, tex2);
        assert_equal(pass2->diffuse_map().texture_id, tex);
    }

    void test_pass_material_set_on_clone() {
        auto material = window->shared_assets->clone_default_material().fetch();

        assert_equal(material->pass(0)->material()->id(), material->id());
    }

    void test_setting_texture_unit_increases_refcount() {
        auto mat = window->shared_assets->new_material().fetch();
        mat->set_pass_count(1);

        auto texture = window->shared_assets->new_texture().fetch();
        assert_equal(texture.use_count(), 2);

        mat->set_diffuse_map(texture->id());

        assert_equal(mat->diffuse_map().texture_id, texture->id());

        assert_equal(texture.use_count(), 3);
    }

    // FIXME: Restore this
    void test_reflectiveness() {
        /*
        smlt::MaterialID mid = window->shared_assets->new_material();
        auto mat = window->shared_assets->material(mid);
        mat->set_pass_count(1);

        auto pass = mat->pass(0);

        assert_false(pass->is_reflective());
        assert_false(mat->has_reflective_pass());
        assert_equal(0.0, pass->albedo());
        assert_equal(0, pass->reflection_texture_unit());

        pass->set_albedo(0.5);

        assert_equal(0.5, pass->albedo());
        assert_true(pass->is_reflective());
        assert_true(mat->has_reflective_pass());  */
    }
};

#endif // TEST_MATERIAL_H
