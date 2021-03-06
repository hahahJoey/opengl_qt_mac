//
// Created by zhaojunhe on 2018/12/10.
//
#include <util/color.hpp>
#include <array>
#include "color.hpp"
#include <util/csscolorparser.hpp>


namespace mbgl {
std::string Color::stringify() const {
    std::array<double, 4> array = toArray();
    return "rgba(" +
           std::to_string(array[0]) + "," +
           std::to_string(array[1]) + "," +
           std::to_string(array[2]) + "," +
           std::to_string(array[3]) + ")";
}

std::array<double, 4> Color::toArray() const {
    if (a == 0) {
        return {{0, 0, 0, 0}};
    } else {
        return {{r * 255 / a, g * 255 / a, b * 255 / a, a}};
    }
}

optional<Color> Color::parse(const std::string &s) {
    auto css_color = CSSColorParser::parse(s);

    // Premultiply the color.
    if (css_color) {
        const float factor = css_color->a / 255;
        return {{
                        css_color->r * factor,
                        css_color->g * factor,
                        css_color->b * factor,
                        css_color->a
                }};
    } else {
        return {};
    }
}
}

