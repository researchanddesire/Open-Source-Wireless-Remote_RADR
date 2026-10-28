#pragma once

#include <cstddef>
#include <cstdint>

// Button label layout. No hardware dependencies, so it is tested natively.
namespace text_fit {

// Horizontal room kept between a label and the button edges.
constexpr int kButtonTextPadding = 8;

// The Adafruit GFX built-in font is 5x7 in a 6x8 cell at size 1.
constexpr int kBuiltinCharWidth = 6;
constexpr int kBuiltinCharHeight = 8;

/// True when a label measured in the button font fits inside the button.
inline bool fitsButton(int textWidth, int buttonWidth) {
    return textWidth <= buttonWidth - kButtonTextPadding;
}

inline int builtinTextWidth(size_t characters) {
    return static_cast<int>(characters) * kBuiltinCharWidth;
}

/// The built-in font's cursor is the top-left corner, not the baseline.
inline int builtinTextTop(int y, int height) {
    return y + (height - kBuiltinCharHeight) / 2;
}

}  // namespace text_fit
