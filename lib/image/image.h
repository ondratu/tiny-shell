#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tiny::image {

/// The biggest icon that is asked for: hicolor and flatpak have up to 512 px.
constexpr int MAX_ICON_SIZE = 512;

/// The biggest bitmap that is read (an icon is sometimes drawn bigger than
/// it is shown): 1024 x 1024 pixels are 4 MB.
constexpr int MAX_SOURCE_SIZE = 1024;

/// A decoded picture: premultiplied ARGB, the format of Xrender.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels; //!< row by row, width * height

    /// A size of at least 1x1 and the pixels of exactly that size.
    bool valid() const
    {
        return width >= 1 && height >= 1 &&
               pixels.size() ==
                       static_cast<size_t>(width) * static_cast<size_t>(height);
    }

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
 * unsupported file, for a `size` out of 1..MAX_ICON_SIZE, for a bitmap
 * bigger than MAX_SOURCE_SIZE and for a text file (SVG, XPM) of megabytes.
 */
std::optional<Image> load_image(const std::string& path, int size);

/**
 * @short Scale the picture so that its longer side is `size` pixels.
 *
 * The aspect ratio is kept (the shorter side is at least 1 pixel), up or down,
 * Up it uses a bilinear filter, down the average of the covered pixels (a box
 * filter, so thin lines do not vanish), both of the premultiplied pixels. A picture that already
 * has the size is copied. Returns nothing for an empty picture or a `size` out
 * of 1..MAX_ICON_SIZE.
 */
std::optional<Image> scale_image(const Image& image, int size);

} // namespace tiny::image
