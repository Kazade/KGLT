#pragma once

#include <cstdint>
#include "../../colour.h"

namespace smlt {
namespace ui {

struct UInt4 {
    uint16_t left;
    uint16_t right;
    uint16_t bottom;
    uint16_t top;
};

struct UICoord {
    UICoord():
        x(0), y(0) {}

    UICoord(uint16_t x, uint16_t y):
        x(x), y(y) {}

    bool operator==(const UICoord& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    uint16_t x, y;
};

enum OverflowType {
    OVERFLOW_TYPE_HIDDEN,
    OVERFLOW_TYPE_VISIBLE,
    OVERFLOW_TYPE_AUTO
};

enum TextAlignment {
    TEXT_ALIGNMENT_LEFT,
    TEXT_ALIGNMENT_CENTER,
    TEXT_ALIGNMENT_RIGHT
};

enum ResizeMode {
    RESIZE_MODE_FIXED, // Clips / scrolls text
    RESIZE_MODE_FIXED_WIDTH, // Will expand vertically with text, text is word-wrapped
    RESIZE_MODE_FIXED_HEIGHT, // Will expand horizontally with text, text is not wrapped
    RESIZE_MODE_FIT_CONTENT // Will fit the text, newlines affect the height
};

enum ChangeFocusBehaviour {
    FOCUS_THIS_IF_NONE_FOCUSED = 0x1,
    FOCUS_NONE_IF_NONE_FOCUSED = 0x2
};

struct UIDim {
    float width = 0.0;
    float height = 0.0;
};


struct UIConfig {
    static const Colour ALICE_BLUE;
    static const Colour LIGHT_GREY;
    static const Colour DODGER_BLUE;

    uint16_t font_size_ = 16;
    uint16_t line_height_ = 18;

    Colour foreground_colour_ = Colour::BLACK;
    Colour background_colour_ = Colour::WHITE;

    ResizeMode label_resize_mode_ = RESIZE_MODE_FIT_CONTENT;
    ResizeMode button_resize_mode_ = RESIZE_MODE_FIXED_HEIGHT;
    ResizeMode progress_bar_resize_mode_ = RESIZE_MODE_FIXED;

    uint8_t scrollbar_width_ = 16;
    Colour scrollbar_background_colour_ = LIGHT_GREY;
    Colour scrollbar_foreground_colour_ = ALICE_BLUE;

    uint16_t button_height_ = 36;
    uint16_t button_width_ = 0; // Fit content

    UInt4 label_padding_ = { 5, 5, 5, 5 };
    PackedColour4444 label_background_colour_ = Colour::NONE;
    PackedColour4444 label_foreground_colour_ = Colour::NONE;
    PackedColour4444 label_border_colour_ = Colour::NONE;
    PackedColour4444 label_text_colour_ = DODGER_BLUE;

    UInt4 button_padding_ = { 30, 30, 20, 20 };
    PackedColour4444 button_background_colour_ = DODGER_BLUE;
    PackedColour4444 button_foreground_colour_ = Colour::NONE;
    PackedColour4444 button_text_colour_ = Colour::WHITE;
    PackedColour4444 button_border_colour_ = Colour::NONE;

    uint16_t button_border_width_ = 0;
    uint16_t button_border_radius_ = 3;

    PackedColour4444 progress_bar_foreground_colour_ = DODGER_BLUE;
    PackedColour4444 progress_bar_background_colour_ = Colour::WHITE;
    PackedColour4444 progress_bar_border_colour_ = DODGER_BLUE;
    float progress_bar_border_width_ = 1;
    uint16_t progress_bar_width_ = 100;
    uint16_t progress_bar_height_ = 16;

    OverflowType default_overflow_ = OVERFLOW_TYPE_HIDDEN;
    ResizeMode default_resize_mode_ = RESIZE_MODE_FIXED;
};

}
}
