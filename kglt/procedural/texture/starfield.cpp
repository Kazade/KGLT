#include "../../texture.h"
#include "../utils/simplex_noise.h"
#include "starfield.h"
#include "../../kazbase/random.h"

namespace kglt {
namespace procedural {
namespace texture {

void draw_circle(kglt::Texture& texture, float x, float y, float size, float brightness, const kglt::Colour& colour) {
    float radius = size * 0.5f;

    uint32_t start_y = int(y) - radius - 1;
    uint32_t end_y = int(y) + radius + 1;

    uint32_t start_x = int(x) - radius - 1;
    uint32_t end_x = int(x) + radius + 1;

    int32_t bytes_per_pixel = texture.bpp() / 8;

    for(uint32_t j = start_y; j < end_y; ++j) {
        for(uint32_t i = start_x; i < end_x; ++i) {
            int dx = x - i;
            int dy = y - j;

            if((dx*dx + dy*dy) <= (radius * radius)) {
                int32_t idx = (j * texture.width()) + i;

                texture.data()[idx * bytes_per_pixel] = brightness * colour.r;
                texture.data()[(idx * bytes_per_pixel) + 1] = brightness * colour.g;
                texture.data()[(idx * bytes_per_pixel) + 2] = brightness * colour.b;
                if(bytes_per_pixel == 4) {
                    texture.data()[(idx * bytes_per_pixel) + 3] = brightness * colour.a;
                }
            }
        }
    }
}

void starfield(kglt::Texture& texture, uint32_t width, uint32_t height) {
    seed();

    std::vector<float> density(width * height, 0);

    texture.resize(width, height);
    texture.set_bpp();

    const float GLOBAL_DENSITY = 0.05f;
    const float MAX_SIZE = 2.0;
    const float MAX_BRIGHTNESS = 255;

    for(uint32_t y = 0; y < height; ++y) {
        for(uint32_t x = 0; x < width; ++x) {
            density[(y * width) + x] = fabs(Simplex::noise(float(x), float(y)));
        }
    }

    for(uint32_t y = 0; y < height; ++y) {
        for(uint32_t x = 0; x < width; ++x) {
            float this_density = density[(y * width) + x];
            if(random_float(0, 1) < this_density * GLOBAL_DENSITY) {
                float weight = random_float(0, 1) * this_density;
                float size = weight * MAX_SIZE;
                float brightness = weight * MAX_BRIGHTNESS;

                kglt::Colour colour = kglt::Colour::white;
                float col_rand = random_float(0, 1);
                if(col_rand < 0.03) {
                    colour = kglt::Colour::orange;
                } else if(col_rand < 0.05) {
                    colour = kglt::Colour::yellow;
                }
                draw_circle(texture, x, y, size, brightness, colour);
            }
        }
    }

    texture.upload(false, true, false, false);
}

}
}
}

