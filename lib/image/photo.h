#pragma once

#include <optional>
#include <string>

#include "image/image.h"

namespace tiny::image {

/// The most pixels of a photo that is read (a 5K screen has 14.7 million).
constexpr int MAX_PHOTO_PIXELS = 25'000'000;

/// The biggest file of a photo that is read.
constexpr long MAX_PHOTO_FILE = 64L << 20;

/**
 * @short Load a PNG or a JPEG file (by its content, not by the extension).
 *
 * A photo is bigger than an icon, so this is a separate decoder from
 * `load_image` (it is not linked into the programs that do not use it).
 * Returns nothing for a missing, broken or unsupported file (progressive
 * JPEG is supported, arithmetic coding is not), for a file over
 * MAX_PHOTO_FILE bytes and for a picture of more than MAX_PHOTO_PIXELS or a
 * side over MAX_RESIZE_SIDE. The pixels keep their alpha (see `fit_photo`).
 */
std::optional<Image> load_photo(const std::string& path);

//! How a photo is put on a screen
enum class Fit {
    Fill, //!< scaled to cover the screen, the overhang is cut (the middle stays)
    Center, //!< not scaled, in the middle; black around, or cut
    Tile    //!< not scaled, repeated from the top left corner
};

/**
 * @short The photo as it covers a screen of `width` x `height` pixels.
 *
 * The result has exactly that size and is opaque (the alpha of every pixel is
 * 255, the transparent part is black). Returns nothing for an empty photo or
 * a side out of 1..MAX_RESIZE_SIDE.
 */
std::optional<Image> fit_photo(const Image& photo, int width, int height,
                               Fit fit);

} // namespace tiny::image
