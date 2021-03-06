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

#ifndef TEXTURE_H_INCLUDED
#define TEXTURE_H_INCLUDED

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>
#include "generic/identifiable.h"
#include "generic/managed.h"
#include "loadable.h"
#include "types.h"
#include "asset.h"
#include "interfaces.h"
#include "interfaces/updateable.h"
#include "path.h"

namespace smlt {

enum MipmapGenerate {
    MIPMAP_GENERATE_NONE,
    MIPMAP_GENERATE_COMPLETE
};

enum TextureWrap {
    TEXTURE_WRAP_REPEAT,
    TEXTURE_WRAP_CLAMP_TO_EDGE,
    TEXTURE_WRAP_MIRRORED_REPEAT,
    TEXTURE_WRAP_MIRRORED_CLAMP_TO_EDGE
};

enum TextureFilter {
    TEXTURE_FILTER_POINT,
    TEXTURE_FILTER_BILINEAR,
    TEXTURE_FILTER_TRILINEAR
};

/*
 * Simulant intentionally only supports a handful of formats for portability.
 *
 * This list isn't fixed though, if you need more, just file an issue.
 *
 * Format is:
 *
 * {ORDER}_{COUNT}{TYPE}_{LAYOUT}_{COMPRESSION}_{TWIDDLED}
 *
 * Where TYPE is UB (unsigned byte), US (unsigned short)
 * or UI (unsigned int)
 *
 * In some compressed formats the count+type don't make sense
 * in which case they are omitted.
 */
enum TextureFormat {
    // Standard formats
    TEXTURE_FORMAT_R_1UB_8,
    TEXTURE_FORMAT_RGB_3UB_888,
    TEXTURE_FORMAT_RGBA_4UB_8888,

    // Packed short formats
    TEXTURE_FORMAT_RGB_1US_565,
    TEXTURE_FORMAT_RGBA_1US_4444,
    TEXTURE_FORMAT_RGBA_1US_5551,
    TEXTURE_FORMAT_ARGB_1US_1555,
    TEXTURE_FORMAT_ARGB_1US_4444,
    TEXTURE_FORMAT_RGB_1US_565_TWID,
    TEXTURE_FORMAT_ARGB_1US_4444_TWID,
    TEXTURE_FORMAT_ARGB_1US_1555_TWID,

    // Dreamcast PVR VQ compressed
    TEXTURE_FORMAT_RGB_1US_565_VQ_TWID,
    TEXTURE_FORMAT_ARGB_1US_4444_VQ_TWID,
    TEXTURE_FORMAT_ARGB_1US_1555_VQ_TWID,

    // PVR VQ Compressed but with mipmap data included
    TEXTURE_FORMAT_RGB_1US_565_VQ_TWID_MIP,
    TEXTURE_FORMAT_ARGB_1US_4444_VQ_TWID_MIP,
    TEXTURE_FORMAT_ARGB_1US_1555_VQ_TWID_MIP,

    TEXTURE_FORMAT_INVALID
};

std::size_t texture_format_stride(TextureFormat format);
std::size_t texture_format_channels(TextureFormat format);

/** Returns true if the data in this format contains mipmap
 * data following the main texture data */
bool texture_format_contains_mipmaps(TextureFormat format);

enum TextureFreeData {
    TEXTURE_FREE_DATA_NEVER,
    TEXTURE_FREE_DATA_AFTER_UPLOAD
};

class Renderer;

enum TextureChannel {
    TEXTURE_CHANNEL_RED,
    TEXTURE_CHANNEL_GREEN,
    TEXTURE_CHANNEL_BLUE,
    TEXTURE_CHANNEL_ALPHA,
    TEXTURE_CHANNEL_ZERO,
    TEXTURE_CHANNEL_ONE
};

typedef std::array<TextureChannel, 4> TextureChannelSet;

class Texture :
    public Asset,
    public Loadable,
    public generic::Identifiable<TextureID>,
    public RefCounted<Texture>,
    public Updateable,
    public RenderTarget,
    public ChainNameable<Texture> {

public:
    static const TextureChannelSet DEFAULT_SOURCE_CHANNELS;

    struct BuiltIns {
        static const std::string CHECKERBOARD;
        static const std::string BUTTON;
    };

    typedef std::shared_ptr<Texture> ptr;
    typedef std::vector<uint8_t> Data;

    Texture(TextureID id, AssetManager* asset_manager, uint16_t width, uint16_t height, TextureFormat format=TEXTURE_FORMAT_RGBA_4UB_8888);

    TextureFormat format() const;
    void set_format(TextureFormat format);

    /** Convert a texture to a new format and allow manipulating/filling the channels during the conversion */
    void convert(
        TextureFormat new_format,
        const TextureChannelSet& channels=Texture::DEFAULT_SOURCE_CHANNELS
    );

    /**
     * Change the width and height, but manually set the data buffer size,
     * mainly used for compressed textures
     */
    void resize(uint16_t width, uint16_t height, uint32_t data_size);

    /**
     * Change the width and height, automatically resizing the data buffer
     * depending on the bytes_per_pixel of the texel_type
     */
    void resize(uint16_t width, uint16_t height);

    /**
     * Flip the data buffer vertically. This will have no effect if the
     * data buffer has been wiped after upload
     */
    void flip_vertically();

    void set_source(const smlt::Path& source);

    /** Texture filtering and wrapping */
    void set_texture_filter(TextureFilter filter);

    /** If set to TEXTURE_FREE_DATA_AFTER_UPLOAD then the data attribute will be
     * wiped after the renderer has uploaded to the GPU.
     */
    void set_free_data_mode(TextureFreeData mode);

    /** Set the texture wrap modes, either together or per-dimension */
    void set_texture_wrap(TextureWrap wrap_u, TextureWrap wrap_v, TextureWrap wrap_w);
    void set_texture_wrap_u(TextureWrap wrap_u);
    void set_texture_wrap_v(TextureWrap wrap_v);
    void set_texture_wrap_w(TextureWrap wrap_w);

    /** If enabled (default) the texture will be uploaded to the GPU
     * by the renderer. You can disable this if you need just a way
     * to load images from disk for other purposes (e.g. heightmaps)
     */
    void set_auto_upload(bool v=true);
    void set_mipmap_generation(MipmapGenerate type);

    const Texture::Data& data() const;

    /** The required size that data() should be to hold a texture in this format with these dimensions.
      * For non-compressed formats this is usually the width * height * stride. For compressed formats
      * this can vary, and will include any space for things like codebooks.
    */
    static std::size_t required_data_size(TextureFormat fmt, uint16_t width, uint16_t height);

    void set_data(const uint8_t* data, std::size_t size);
    void set_data(const Texture::Data& data);

    /** Clear the data buffer */
    void free();

    /** Returns true if the data array isn't empty */
    bool has_data() const;

    /**
     * Flushes texture data / properties to the renderer immediately. This
     * will free ram if the free data mode is set to TEXTURE_FREE_DATA_AFTER_UPLOAD
     */
    void flush();

    typedef std::function<void (uint8_t*, uint16_t, uint16_t, TextureFormat)> MutationFunc;

    /** Apply a mutation function to the current texture data */
    void mutate_data(MutationFunc func);

    uint16_t width() const override;
    uint16_t height() const override;
    Vec2 dimensions() const { return Vec2(width(), height()); }

    /**
     * Returns true if this Texture uses a compressed format
     */
    bool is_compressed() const;

    /**
     * Returns the number of channels that this texture has
     */
    uint8_t channels() const;

    /**
     * Save a texture to the specified file. Will only work for
     * uncompressed Textures
     */
    void save_to_file(const Path& filename);

    Path source() const;
    TextureFilter texture_filter() const;
    TextureWrap wrap_u() const;
    TextureWrap wrap_v() const;
    TextureWrap wrap_w() const;
    MipmapGenerate mipmap_generation() const;
    TextureFreeData free_data_mode() const;

    /** These are overridden to notify the renderer of texture changes */
    bool init() override;
    void clean_up() override;
    void update(float dt) override;

    /** Returns true if the format contains mipmap data, or mipmaps
     * have been generated during texture upload */
    bool has_mipmaps() const;

    bool auto_upload() const;

    /** This is for storing the GL (or whatever) texture ID */
    void _set_renderer_specific_id(const uint32_t id);
    uint32_t _renderer_specific_id() const;

    /** INTERNAL: Clears the params dirty flag */
    void _set_params_clean();

    /** INTERNAL: returns true if the data needs re-uploading */
    bool _data_dirty() const;

    /** INTERNAL: clears the dirty data flag */
    void _set_data_clean();

    /** INTERNAL: returns true if the filters are dirty */
    bool _params_dirty() const;
    void _set_has_mipmaps(bool v);
private:
    Renderer* renderer_ = nullptr;

    uint16_t width_ = 0;
    uint16_t height_ = 0;

    TextureFormat format_ = TEXTURE_FORMAT_RGBA_4UB_8888;

    Path source_;

    bool auto_upload_ = true; /* If true, the texture is uploaded by the renderer asap */
    bool data_dirty_ = true;
    Texture::Data data_;
    TextureFreeData free_data_mode_ = TEXTURE_FREE_DATA_AFTER_UPLOAD;

    MipmapGenerate mipmap_generation_ = MIPMAP_GENERATE_COMPLETE;
    bool has_mipmaps_ = false;

    bool params_dirty_ = true;
    TextureFilter filter_ = TEXTURE_FILTER_POINT;
    TextureWrap wrap_u_ = TEXTURE_WRAP_REPEAT;
    TextureWrap wrap_v_ = TEXTURE_WRAP_REPEAT;
    TextureWrap wrap_w_ = TEXTURE_WRAP_REPEAT;

    uint32_t renderer_id_ = 0;
};

}

#endif // TEXTURE_H_INCLUDED
