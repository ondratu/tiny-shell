#pragma once

#include <optional>
#include <string_view>

#include "image/image.h"

namespace tiny::image {

/**
 * @short Parse the text of an XPM (version 3) file.
 *
 * Colors are `None` (transparent), `#RGB` in 4, 8, 12 or 16 bits per
 * channel and a small table of the common X11 color names, including
 * `grayNN`. A color that is not known is transparent. Returns nothing for a
 * text that is not a valid XPM.
 */
std::optional<Image> parse_xpm(std::string_view text);

} // namespace tiny::image
