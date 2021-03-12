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

#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#include "colour.h"

namespace smlt {

const Colour Colour::BLACK = Colour(0.0, 0.0, 0.0, 1.0);
const Colour Colour::BLUE = Colour(0.0, 0.0, 1.0, 1.0);
const Colour Colour::GREEN = Colour(0.0, 1.0, 0.0, 1.0);
const Colour Colour::RED = Colour(1.0, 0.0, 0.0, 1.0);
const Colour Colour::WHITE = Colour(1.0, 1.0, 1.0, 1.0);
const Colour Colour::NONE = Colour(0, 0, 0, 0); // Transparent
const Colour Colour::YELLOW = Colour(1.0f, 1.0f, 0.0f, 1.0f);
const Colour Colour::PURPLE = Colour(1.0f, 0.0f, 1.0f, 1.0f);
const Colour Colour::TURQUOISE = Colour(0.0f, 1.0f, 1.0f, 1.0f);
const Colour Colour::GREY = Colour(0.5f, 0.5f, 0.5f, 1.0f);

std::string Colour::to_hex_string() const {
    auto rval = int(255.0f * r);
    auto gval = int(255.0f * g);
    auto bval = int(255.0f * b);
    auto aval = int(255.0f * a);

    std::string final;

    for(auto& val: {rval, gval, bval, aval}) {
        std::stringstream sstream;
        sstream << std::hex << std::setw(2) << std::setfill('0') << val;
        final += sstream.str();
    }

    return final;
}

Colour Colour::from_hex_string(const std::string& hex_string) {
    std::string rpart(hex_string.begin(), hex_string.begin() + 2);
    std::string gpart(hex_string.begin() + 2, hex_string.begin() + 4);
    std::string bpart(hex_string.begin() + 4, hex_string.begin() + 6);
    std::string apart(hex_string.begin() + 6, hex_string.end());

    return Colour(
        float(strtoul(rpart.c_str(), nullptr, 16)) / 255.0f,
        float(strtoul(gpart.c_str(), nullptr, 16)) / 255.0f,
        float(strtoul(bpart.c_str(), nullptr, 16)) / 255.0f,
        float(strtoul(apart.c_str(), nullptr, 16)) / 255.0f
    );
}

std::ostream& operator<<(std::ostream& stream, const Colour& c) {
    stream << "#" << c.to_hex_string();
    return stream;
}

Colour Colour::lerp(const Colour& end, float t) const {
    t = std::min(t, 1.0f);
    t = std::max(t, 0.0f);

    return *this + ((end - *this) * t);
}


}
