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

#include <cassert>
#include <cstring>
#include <stdexcept>

#include "utils/gl_thread_check.h"
#include "utils/gl_error.h"

#include "logging.h"

#include "window.h"
#include "texture.h"
#include "asset_manager.h"
#include "renderers/renderer.h"

namespace smlt {

const std::string Texture::BuiltIns::CHECKERBOARD = "simulant/textures/checkerboard.png";
const std::string Texture::BuiltIns::BUTTON = "simulant/textures/button.png";

const TextureChannelSet Texture::DEFAULT_SOURCE_CHANNELS = {{
    TEXTURE_CHANNEL_RED,
    TEXTURE_CHANNEL_GREEN,
    TEXTURE_CHANNEL_BLUE,
    TEXTURE_CHANNEL_ALPHA
}};

bool texture_format_contains_mipmaps(TextureFormat format) {
    switch(format) {
        case TEXTURE_FORMAT_ARGB_1US_1555_VQ_TWID_MIP:
        case TEXTURE_FORMAT_ARGB_1US_4444_VQ_TWID_MIP:
        case TEXTURE_FORMAT_RGB_1US_565_VQ_TWID_MIP:
            return true;
        default:
            return false;
    }
}

std::size_t texture_format_channels(TextureFormat format) {
    switch(format) {
    case TEXTURE_FORMAT_R_1UB_8:
        return 1;
    case TEXTURE_FORMAT_RGB_1US_565:
    case TEXTURE_FORMAT_RGB_1US_565_TWID:
    case TEXTURE_FORMAT_RGB_3UB_888:
    case TEXTURE_FORMAT_RGB_1US_565_VQ_TWID:
        return 3;
    case TEXTURE_FORMAT_RGBA_4UB_8888:
    case TEXTURE_FORMAT_RGBA_1US_4444:
    case TEXTURE_FORMAT_ARGB_1US_4444:
    case TEXTURE_FORMAT_ARGB_1US_4444_TWID:
    case TEXTURE_FORMAT_RGBA_1US_5551:
    case TEXTURE_FORMAT_ARGB_1US_1555:
    case TEXTURE_FORMAT_ARGB_1US_1555_TWID:
    case TEXTURE_FORMAT_ARGB_1US_4444_VQ_TWID:
    case TEXTURE_FORMAT_ARGB_1US_1555_VQ_TWID:
        return 4;
    default:
        S_ERROR("Invalid TextureFormat!");
        return 0;
    }
}

Texture::Texture(TextureID id, AssetManager *asset_manager, uint16_t width, uint16_t height, TextureFormat format):
    Asset(asset_manager),
    generic::Identifiable<TextureID>(id) {

    S_DEBUG("Creating texture {0}x{1}", width, height);
    resize(width, height);

    S_DEBUG("Setting format to: {0}", format);
    set_format(format);

    /* We intentionally don't mark data dirty here. All that would happen is
     * we would upload a blank texture */

    renderer_ = asset_manager->window->renderer;
}

TextureFormat Texture::format() const {
    return format_;
}

uint16_t Texture::width() const {
    return width_;
}

uint16_t Texture::height() const {
    return height_;
}

std::size_t Texture::required_data_size(TextureFormat fmt, uint16_t width, uint16_t height) {
    switch(fmt) {
        case TEXTURE_FORMAT_RGB_1US_565_VQ_TWID:
        case TEXTURE_FORMAT_ARGB_1US_4444_VQ_TWID:
        case TEXTURE_FORMAT_ARGB_1US_1555_VQ_TWID:
            /* 2048 byte codebook, 8bpp per 2x2 */
            return 2048 + ((width / 2) * (height / 2));
        default:
            break;
    }

    return texture_format_stride(fmt) * width * height;
}

void Texture::set_format(TextureFormat format) {
    if(format_ == format) {
        return;
    }

    format_ = format;

    data_.resize(required_data_size(format, width_, height_));
    data_.shrink_to_fit();

    data_dirty_ = true;
}

void Texture::resize(uint16_t width, uint16_t height, uint32_t data_size) {
    if(width_ == width && height_ == height && data_.size() == data_size) {
        return;
    }

    width_ = width;
    height_ = height;
    data_.resize(data_size);
    data_.shrink_to_fit();

    data_dirty_ = true;
}

void Texture::resize(uint16_t width, uint16_t height) {
    if(width_ == width && height_ == height) {
        return;
    }

    width_ = width;
    height_ = height;

    data_.resize(required_data_size(format_, width, height));
    data_.shrink_to_fit();

    data_dirty_ = true;
}

static void explode_r8(const uint8_t* source, const TextureChannelSet& channels, float& r, float& g, float& b, float& a) {
    float sr = float(*source) / 255.0f;

    auto calculate_component = [&channels](uint8_t i, float sr, float sg, float sb, float sa) -> float {
        switch(channels[i]) {
            case TEXTURE_CHANNEL_ZERO: return 0.0f;
            case TEXTURE_CHANNEL_ONE: return 1.0f;
            case TEXTURE_CHANNEL_RED: return sr;
            case TEXTURE_CHANNEL_GREEN: return sg;
            case TEXTURE_CHANNEL_BLUE: return sb;
            case TEXTURE_CHANNEL_ALPHA: return sa;
        }

        return 0.0f;
    };

    r = calculate_component(0, sr, 0, 0, 0);
    g = calculate_component(1, sr, 0, 0, 0);
    b = calculate_component(2, sr, 0, 0, 0);
    a = calculate_component(3, sr, 0, 0, 0);
}

static void compress_rgba4444(uint8_t* dest, float r, float g, float b, float a) {
    uint16_t* out = (uint16_t*) dest;

    uint8_t rr = (uint8_t) (15.0f * r);
    uint8_t rg = (uint8_t) (15.0f * g);
    uint8_t rb = (uint8_t) (15.0f * b);
    uint8_t ra = (uint8_t) (15.0f * a);

    *out = (rr << 12) | (rg << 8) | (rb << 4) | ra;
}

static void compress_rgba8888(uint8_t* dest, float r, float g, float b, float a) {
    uint32_t* out = (uint32_t*) dest;

    uint8_t rr = (uint8_t) float(255) * r;
    uint8_t rg = (uint8_t) float(255) * g;
    uint8_t rb = (uint8_t) float(255) * b;
    uint8_t ra = (uint8_t) float(255) * a;

    *out = (rr << 24) | (rg << 16) | (rb << 8) | ra;
}

static void explode_rgba8888(const uint8_t* source, const TextureChannelSet& channels, float& r, float& g, float& b, float& a) {
    auto calculate_component = [&channels](uint8_t i, float sr, float sg, float sb, float sa) -> float {
        switch(channels[i]) {
            case TEXTURE_CHANNEL_ZERO: return 0.0f;
            case TEXTURE_CHANNEL_ONE: return 1.0f;
            case TEXTURE_CHANNEL_RED: return sr;
            case TEXTURE_CHANNEL_GREEN: return sg;
            case TEXTURE_CHANNEL_BLUE: return sb;
            case TEXTURE_CHANNEL_ALPHA: return sa;
        }

        return 0.0f;
    };

    const float inv = 1.0f / 255.0f;

    float sr = float(source[0]) * inv;
    float sg = float(source[1]) * inv;
    float sb = float(source[2]) * inv;
    float sa = float(source[3]) * inv;

    r = calculate_component(0, sr, sg, sb, sa);
    g = calculate_component(1, sr, sg, sb, sa);
    b = calculate_component(2, sr, sg, sb, sa);
    a = calculate_component(3, sr, sg, sb, sa);
}

typedef void (*ExplodeFunc)(const uint8_t*, const TextureChannelSet&, float&, float&, float&, float&);
typedef void (*CompressFunc)(uint8_t*, float, float, float, float);

static const std::map<TextureFormat, ExplodeFunc> EXPLODERS = {
    {TEXTURE_FORMAT_R_1UB_8, explode_r8},
    {TEXTURE_FORMAT_RGBA_4UB_8888, explode_rgba8888}
};

static const std::map<TextureFormat, CompressFunc> COMPRESSORS = {
    {TEXTURE_FORMAT_RGBA_1US_4444, compress_rgba4444},
    {TEXTURE_FORMAT_RGBA_4UB_8888, compress_rgba8888}
};

void Texture::convert(TextureFormat new_format, const TextureChannelSet &channels) {
    auto original_data = data();
    auto original_format = format();

    set_format(new_format);

    mutate_data([=](uint8_t* data, uint16_t, uint16_t, TextureFormat nf) {
        _S_UNUSED(nf);
        assert(nf == new_format); // Make sure the new format was applied

        auto source_stride = texture_format_stride(original_format);
        auto dest_stride = texture_format_stride(new_format);

        const uint8_t* source_ptr = &original_data[0];
        uint8_t* dest_ptr = &data[0];

        if(!EXPLODERS.count(original_format) || !COMPRESSORS.count(new_format)) {
            throw std::logic_error("Unsupported texture conversion");
        }

        auto explode = EXPLODERS.at(original_format);
        auto compress = COMPRESSORS.at(new_format);

        for(auto i = 0u; i < original_data.size(); i += source_stride, source_ptr += source_stride, dest_ptr += dest_stride) {
            float r, g, b, a;

            explode(source_ptr, channels, r, g, b, a);
            compress(dest_ptr, r, g, b, a);
        }
    });
}


static void do_flip_vertically(uint8_t* data, uint16_t width, uint16_t height, TextureFormat format) {
    /**
     *  Flips the texture data vertically
     */
    auto w = (uint32_t) width;
    auto h = (uint32_t) height;
    auto c = (uint32_t) texture_format_channels(format);

    auto row_size = w * c;

    uint8_t* src_row = &data[0];
    uint8_t* dst_row = &data[(h - 1) * row_size];
    std::vector<char> tmp(row_size);

    for(auto i = 0u; i < h / 2; ++i) {
        if(src_row != dst_row) {
            memcpy(&tmp[0], src_row, row_size);
            memcpy(src_row, dst_row, row_size);
            memcpy(dst_row, &tmp[0], row_size);
        }

        src_row += row_size;
        dst_row -= row_size;
    }
}

void Texture::flip_vertically() {
    mutate_data(&do_flip_vertically);
}

void Texture::set_source(const Path& source) {
    source_ = source;
}

void Texture::free() {
    data_.clear();
    data_.shrink_to_fit();

    /* We don't mark data dirty here, we don't want
     * anything to be updated in GL, we're just freeing
     * the RAM */
}

bool Texture::has_data() const {
    return !data_.empty();
}

void Texture::flush() {
    if(cort::within_coroutine()) {
        renderer_->window_->idle->add_once([this]() {
            renderer_->prepare_texture(this);
        });
        cr_yield();
    } else {
        renderer_->prepare_texture(this);
    }
}

void Texture::mutate_data(Texture::MutationFunc func) {
    func(&data_[0], width_, height_, format_);

    /* A mutation by definition updates the data */
    data_dirty_ = true;
}

bool Texture::is_compressed() const {
    switch(format_) {
    case TEXTURE_FORMAT_RGB_1US_565_VQ_TWID:
    case TEXTURE_FORMAT_ARGB_1US_4444_VQ_TWID:
    case TEXTURE_FORMAT_ARGB_1US_1555_VQ_TWID:
        return true;
    default:
        return false;
    }
}

uint8_t Texture::channels() const {
    return texture_format_channels(format_);
}

const Texture::Data &Texture::data() const {
    return data_;
}

void Texture::set_data(const uint8_t* data, std::size_t size) {
    data_.assign(data, data + size);
}

void Texture::save_to_file(const Path& filename) {
    _S_UNUSED(filename);
    assert(0 && "Not Implemented");
}

Path Texture::source() const {
    return source_;
}

TextureFilter Texture::texture_filter() const {
    return filter_;
}

TextureWrap Texture::wrap_u() const {
    return wrap_u_;
}

TextureWrap Texture::wrap_v() const {
    return wrap_v_;
}

TextureWrap Texture::wrap_w() const {
    return wrap_w_;
}

MipmapGenerate Texture::mipmap_generation() const {
    return mipmap_generation_;
}

void Texture::set_texture_filter(TextureFilter filter) {
    if(filter != filter_) {
        filter_ = filter;
        params_dirty_ = true;
    }
}

void Texture::set_free_data_mode(TextureFreeData mode) {
    free_data_mode_ = mode;
    params_dirty_ = true;
}

TextureFreeData Texture::free_data_mode() const {
    return free_data_mode_;
}

bool Texture::_params_dirty() const {
    return params_dirty_;
}

void Texture::_set_params_clean() {
    params_dirty_ = false;
}

bool Texture::_data_dirty() const {
    return data_dirty_;
}

void Texture::_set_data_clean() {
    data_dirty_ = false;
}

void Texture::set_texture_wrap(TextureWrap wrap_u, TextureWrap wrap_v, TextureWrap wrap_w) {
    set_texture_wrap_u(wrap_u);
    set_texture_wrap_v(wrap_v);
    set_texture_wrap_w(wrap_w);
}

void Texture::set_texture_wrap_u(TextureWrap wrap_u) {
    if(wrap_u != wrap_u_) {
        wrap_u_ = wrap_u;
        params_dirty_ = true;
    }
}

void Texture::set_texture_wrap_v(TextureWrap wrap_v) {
    if(wrap_v != wrap_v_) {
        wrap_v_ = wrap_v;
        params_dirty_ = true;
    }
}

void Texture::set_texture_wrap_w(TextureWrap wrap_w) {
    if(wrap_w != wrap_w_) {
        wrap_w_ = wrap_w;
        params_dirty_ = true;
    }
}

void Texture::set_auto_upload(bool v) {
    auto_upload_ = v;
    params_dirty_ = true;
}

void Texture::set_mipmap_generation(MipmapGenerate type) {
    mipmap_generation_ = type;
    params_dirty_ = true;
}

void Texture::set_data(const Texture::Data& d) {
    data_ = d;
    data_dirty_ = true;
}

void Texture::_set_has_mipmaps(bool v) {
    has_mipmaps_ = v;
}

bool Texture::init() {
    // Tell the renderer about the texture
    S_DEBUG("Registering texture with the renderer: {0}", renderer_);
    renderer_->register_texture(id(), this);
    return true;
}

void Texture::clean_up() {
    // Tell the renderer to forget the texture
    renderer_->unregister_texture(id(), this);
}

void Texture::update(float dt) {
    _S_UNUSED(dt);
}

bool Texture::has_mipmaps() const {
    return texture_format_contains_mipmaps(format_) || has_mipmaps_;
}

bool Texture::auto_upload() const {
    return auto_upload_;
}

void Texture::_set_renderer_specific_id(const uint32_t id) {
    renderer_id_ = id;
}

uint32_t Texture::_renderer_specific_id() const {
    return renderer_id_;
}

std::size_t texture_format_stride(TextureFormat format) {
    switch(format) {
        case TEXTURE_FORMAT_R_1UB_8: return 1;
        case TEXTURE_FORMAT_RGB_1US_565:
        case TEXTURE_FORMAT_RGB_1US_565_TWID:
        case TEXTURE_FORMAT_RGBA_1US_4444:
        case TEXTURE_FORMAT_ARGB_1US_4444:
        case TEXTURE_FORMAT_ARGB_1US_4444_TWID:
        case TEXTURE_FORMAT_ARGB_1US_1555:
        case TEXTURE_FORMAT_ARGB_1US_1555_TWID:
        case TEXTURE_FORMAT_RGBA_1US_5551: return 2;
        case TEXTURE_FORMAT_RGB_3UB_888: return 3;
        case TEXTURE_FORMAT_RGBA_4UB_8888: return 4;
    default:
        assert(0 && "Not implemented");
        return 0;
    }
}

}
