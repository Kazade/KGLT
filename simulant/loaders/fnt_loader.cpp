
#include "fnt_loader.h"
#include "../font.h"
#include "../resource_manager.h"

namespace smlt {
namespace loaders {

struct OptionsBitField {
    uint8_t smooth : 1;
    uint8_t unicode : 1;
    uint8_t italic : 1;
    uint8_t bold : 1;
    uint8_t fixed_height : 1;
    uint8_t reserved : 3;
};

struct InfoBlock {
    uint16_t font_size;
    OptionsBitField flags;
    uint8_t charset;
    uint16_t stretch_h;
    uint8_t aa;
    uint8_t padding_up;
    uint8_t padding_right;
    uint8_t padding_down;
    uint8_t padding_left;
    uint8_t horizontal_spacing;
    uint8_t vertical_spacing;
    uint8_t outline;
    char name[256];
};

struct PackedBitField {
    uint8_t reserved : 7;
    uint8_t packed : 1;
};

#pragma pack(1)
struct Common {
    uint16_t line_height;
    uint16_t base;
    uint16_t scale_w;
    uint16_t scale_h;
    uint16_t pages;
    PackedBitField packed;
    uint8_t alpha;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct Char {
    uint32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t xoffset;
    int16_t yoffset;
    int16_t xadvance;
    uint8_t page;
    uint8_t channel;
};
#pragma pack()


void FNTLoader::read_text(Font* font, std::istream& data, const LoaderOptions &options) {
    typedef std::unordered_map<std::string, std::string> Options;

    auto parse_line = [](const unicode& line, std::string& line_type) -> Options {
        Options result;
        auto parts = line.split(" ");
        line_type = parts[0].strip().encode();

        for(auto i = 1u; i < parts.size(); ++i) {
            auto part = parts[i].strip();
            auto lhs_rhs = part.split("=");
            if(lhs_rhs.size() == 2) {
                result[lhs_rhs[0].strip().encode()] = lhs_rhs[1].strip().encode();
            }
        }

        return result;
    };

    data.seekg(0, std::ios::beg);

    std::string page;
    std::string line;
    while(std::getline(data, line)) {
        std::string type;
        auto line_settings = parse_line(line, type);

        if(type == "info") {
            font->font_size_ = std::stoi(line_settings["size"]);
        } else if(type == "common") {
            font->line_gap_ = std::stoi(line_settings["lineHeight"]);
        } else if(type == "page") {
            if(page.empty()) {
                page = line_settings["file"];
                if(page[0] == '"' && page[page.size()-1] == '"') {
                    page = std::string(page.begin() + 1, page.begin() + (page.size() - 1));
                }
            }
        } else if(type == "chars") {

        } else if(type == "char") {
            auto id = std::stoi(line_settings["id"]) - 32;

            assert(id >= 0);

            CharInfo c;
            c.x0 = std::stof(line_settings["x"]);
            c.x1 = c.x0 + std::stoi(line_settings["width"]);
            c.y0 = std::stof(line_settings["y"]);
            c.y1 = c.y0 + std::stoi(line_settings["height"]);

            c.xoff = std::stoi(line_settings["xoffset"]);
            c.yoff = std::stoi(line_settings["yoffset"]);
            c.xadvance = std::stoi(line_settings["xadvance"]);

            // Make sure we can contain this character and
            // assign to the right place
            font->char_data_.resize(id + 1);
            font->char_data_[id] = c;
        } else if(type == "kernings") {

        } else if(type == "kerning") {

        } else {
            L_WARN("Unexpected line type while parsing FNT");
        }
    }

    prepare_texture(font, page);
}

void FNTLoader::read_binary(Font* font, std::istream& data, const LoaderOptions& options) {
    enum BlockType {
        INFO = 1,
        COMMON = 2,
        PAGES = 3,
        CHARS = 4,
        KERNING_PAIRS = 5
    };

    struct BlockHeader {
        uint8_t type;
        uint32_t size;
    };

    InfoBlock info;
    Common common;
    std::vector<std::string> pages;
    std::vector<Char> chars;

    std::memset(info.name, 0, 256);

    while(data.good()) {
        BlockHeader header;
        data.read((char*)&header.type, sizeof(uint8_t));
        data.read((char*)&header.size, sizeof(uint32_t));

        switch(header.type) {
            case INFO: {
                // We allow a name up to 256 characters, this just makes sure that
                // we don't go trashing memory
                assert(header.size < sizeof(InfoBlock));
                data.read((char*) &info, header.size);  // Using size, rather than sizeof(BlockHeader) is important
            } break;
            case COMMON: {
                data.read((char*) &common, sizeof(Common));
            } break;
            case PAGES: {
                /* Pages are a set of null terminated strings, all the same length
                 * so we read the block, then find the full null-char, then we know the
                 * length of all the strings
                 */
                std::vector<char> page_data(header.size);
                data.read(&page_data[0], header.size);
                auto it = std::find(page_data.begin(), page_data.end(), '\0');
                if(it != page_data.end()) {
                    auto length = std::distance(page_data.begin(), it);
                    auto count = header.size / length;

                    for(auto i = 0; i < count; ++i) {
                        auto start = i * length;
                        auto end = (i * length) + length;
                        std::string page(page_data.begin() + start, page_data.begin() + end);
                        pages.push_back(page);
                    }

                } else {
                    throw std::runtime_error("Invalid binary FNT file. Couldn't determine page name length.");
                }
            } break;
            case CHARS: {
                auto char_count = header.size / sizeof(Char);
                chars.resize(char_count);
                data.read((char*) &chars[0], sizeof(Char) * char_count);
            } break;
            case KERNING_PAIRS: {
                // Do nothing with this for now, just skip to the end of the file
                char buffer[header.size];
                data.read(buffer, header.size);
            } break;
        }
    }

    font->line_gap_ = common.line_height;
    font->char_data_.resize(chars.size());
    uint32_t i = 0;;
    for(auto& ch: chars) {
        auto& dst = font->char_data_[i];
        dst.x0 = ch.x;
        dst.x1 = ch.x + ch.width;
        dst.y0 = ch.y;
        dst.y1 = ch.y + ch.height;
        dst.xoff = ch.xoffset;
        dst.yoff = ch.yoffset;
        dst.xadvance = ch.xadvance;
        ++i;
    }

    prepare_texture(font, pages[0]);
}

void FNTLoader::prepare_texture(Font* font, const std::string& texture_file) {
    // FIXME: Support multiple pages
    auto texture_path = kfs::path::dir_name(filename_.encode());
    texture_path = kfs::path::join(texture_path, texture_file);

    font->texture_ = font->resource_manager().new_texture_from_file(texture_path).fetch();
    font->material_ = font->resource_manager().new_material_from_file(Material::BuiltIns::TEXTURE_ONLY).fetch();
    font->material_->set_texture_unit_on_all_passes(0, font->texture_id());

    // Set the page dimensions
    // FIXME: Multiple pages
    font->page_height_ = font->texture_->height();
    font->page_width_ = font->texture_->width();

    font->material_->first_pass()->set_blending(smlt::BLEND_NONE);
    if(font->texture_->channels() == 1) {
        //font->material_->first_pass()->set_blending(smlt::BLEND_COLOUR);
        font->texture_->set_format(TEXTURE_FORMAT_LUMINANCE);
    } else {
        //font->material_->first_pass()->set_blending(smlt::BLEND_ALPHA);
    }
}

void FNTLoader::into(Loadable& resource, const LoaderOptions& options) {
    const char TEXT_MARKER[4] = {'i', 'n', 'f', 'o'};
    const char BINARY_MARKER[4] = {'B', 'M', 'F', '\3'};

    Font* font = loadable_to<Font>(resource);

    char version_details[4];
    data_->read(version_details, sizeof(char) * 4);

    if(std::memcmp(version_details, TEXT_MARKER, 4) == 0) {
        read_text(font, *data_, options);
    } else if(std::memcmp(version_details, BINARY_MARKER, 4) == 0) {
        read_binary(font, *data_, options);
    } else {
        throw std::runtime_error("Unsupported .FNT file");
    }

    /*
    font->info_.reset(new stbtt_fontinfo());
    font->font_size_ = font_size;

    stbtt_fontinfo* info = font->info_.get();

    const std::string buffer_string = this->data_->str();
    const unsigned char* buffer = (const unsigned char*) buffer_string.c_str();
    // Initialize the font data
    stbtt_InitFont(info, buffer, stbtt_GetFontOffsetForIndex(buffer, 0));

    font->scale_ = stbtt_ScaleForPixelHeight(info, (int) font_size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);

    font->ascent_ = float(ascent) * font->scale_;
    font->descent_ = float(descent) * font->scale_;
    font->line_gap_ = float(line_gap) * font->scale_;

    // Generate a new texture for rendering the font to
    auto texture = font->texture_ = font->resource_manager().new_texture().fetch();
    texture->set_format(TEXTURE_FORMAT_RGBA); // Need to use GL_RGBA for Dreamcast

    if(charset != CHARACTER_SET_LATIN) {
        throw std::runtime_error("Unsupported character set - please submit a patch!");
    }

    auto first_char = 32;
    auto char_count = 256 - 32; // Latin-1

    font->char_data_.resize(char_count);

    // Dreamcast needs 32bpp, so we bake the font bitmap here
    // temporarily and then generate a RGBA texture from it

    std::vector<uint8_t> tmp_buffer(TEXTURE_WIDTH * TEXTURE_HEIGHT);
    uint8_t* out_buffer = &tmp_buffer[0];
    stbtt_BakeFontBitmap(
        &buffer[0], 0, font_size, out_buffer,
        TEXTURE_WIDTH, TEXTURE_HEIGHT,
        first_char, char_count,
        &font->char_data_[0]
    );

    L_DEBUG("F: Converting font texture from 8bit -> 32bit");

    // Convert from 8bpp to 32bpp
    {
        // Lock against updates
        auto lock = font->texture_->lock();

        texture->resize(TEXTURE_WIDTH, TEXTURE_HEIGHT);

        // Manipulate the data buffer
        auto data = &texture->data()[0];
        uint32_t i = 0;
        for(auto& b: tmp_buffer) {
            uint32_t idx = i * 4;
            data[idx] = data[idx + 1] = data[idx + 2] = 255;
            data[idx + 3] = b;
            ++i;
        }
        L_DEBUG("F: Finished conversion");

        // Mark the data as changed
        texture->mark_data_changed();
    }

    font->material_ = font->resource_manager().new_material_from_file(Material::BuiltIns::TEXTURE_ONLY).fetch();
    font->material_->first_pass()->set_blending(smlt::BLEND_ALPHA);
    font->material_->set_texture_unit_on_all_passes(0, font->texture_id());
    */

    L_DEBUG("Font loaded successfully");
}
}
}
