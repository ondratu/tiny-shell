#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tiny {

/// A decoded picture: premultiplied ARGB, the format of Xrender.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels; //!< row by row, width * height

    uint32_t at(int x, int y) const
    {
        return pixels[static_cast<size_t>(y) * static_cast<size_t>(width) +
                      static_cast<size_t>(x)];
    }
};

/**
 * @short Load a PNG, an XPM or an SVG file (by the extension).
 *
 * A PNG and an XPM keep their own size. An SVG is rasterized so that its longer side is
 * `size` pixels (the aspect ratio is kept), up or down: the picture is drawn
 * in the final size, not scaled. Returns nothing for a missing, broken or
 * unsupported file and for a `size` out of 1..4096.
 */
std::optional<Image> load_image(const std::string& path, int size);

} // namespace tiny
