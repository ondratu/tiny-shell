#include "image/photo.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

#include "third_party/vendor_photo.h"

namespace tiny::image {

namespace {

constexpr uint32_t OPAQUE = 0xFF000000U;

// The content of a regular file of up to MAX_PHOTO_FILE bytes.
std::optional<std::string> read_file(const std::string& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > static_cast<std::uintmax_t>(MAX_PHOTO_FILE)) {
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(content.data(), static_cast<std::streamsize>(size)) &&
        !file.eof()) {
        return std::nullopt;
    }
    content.resize(static_cast<size_t>(file.gcount()));
    return content;
}

// The part of the picture: x, y and the size must lie inside of it.
Image crop(const Image& image, int x, int y, int width, int height)
{
    Image out;
    out.width = width;
    out.height = height;
    out.pixels.reserve(static_cast<size_t>(width) *
                       static_cast<size_t>(height));
    for (int row = y; row < y + height; ++row) {
        const auto first = image.pixels.begin() +
                           static_cast<std::ptrdiff_t>(row) * image.width + x;
        out.pixels.insert(out.pixels.end(), first, first + width);
    }
    return out;
}

// An opaque picture of `width` x `height` pixels, black.
Image canvas(int width, int height)
{
    Image out;
    out.width = width;
    out.height = height;
    out.pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height),
                      OPAQUE);
    return out;
}

// The photo in the middle of a black canvas (cut when it is bigger).
Image centered(const Image& photo, int width, int height)
{
    Image out = canvas(width, height);
    const int left = (width - photo.width) / 2;
    const int top = (height - photo.height) / 2;
    for (int y = std::max(0, top); y < std::min(height, top + photo.height);
         ++y) {
        for (int x = std::max(0, left); x < std::min(width, left + photo.width);
             ++x) {
            // premultiplied, so the color is already mixed with black
            out.pixels[static_cast<size_t>(y) * static_cast<size_t>(width) +
                       static_cast<size_t>(x)] =
                    photo.at(x - left, y - top) | OPAQUE;
        }
    }
    return out;
}

// The photo repeated from the top left corner.
Image tiled(const Image& photo, int width, int height)
{
    Image out = canvas(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            out.pixels[static_cast<size_t>(y) * static_cast<size_t>(width) +
                       static_cast<size_t>(x)] =
                    photo.at(x % photo.width, y % photo.height) | OPAQUE;
        }
    }
    return out;
}

} // namespace

std::optional<Image> load_photo(const std::string& path)
{
    const auto content = read_file(path);
    if (!content || content->empty()) {
        return std::nullopt;
    }
    int width = 0;
    int height = 0;
    const std::unique_ptr<unsigned char, decltype(&photo_free)> rgba(
            photo_decode(
                    reinterpret_cast<const unsigned char*>(content->data()),
                    static_cast<int>(content->size()), MAX_PHOTO_PIXELS, &width,
                    &height),
            &photo_free);
    if (!rgba || width > MAX_RESIZE_SIDE || height > MAX_RESIZE_SIDE) {
        return std::nullopt;
    }
    return premultiplied(rgba.get(), width, height);
}

std::optional<Image> fit_photo(const Image& photo, int width, int height,
                               Fit fit)
{
    if (!photo.valid() || width < 1 || height < 1 || width > MAX_RESIZE_SIDE ||
        height > MAX_RESIZE_SIDE) {
        return std::nullopt;
    }
    switch (fit) {
        case Fit::Center:
            return centered(photo, width, height);
        case Fit::Tile:
            return tiled(photo, width, height);
        case Fit::Fill:
            break;
    }

    // cut the part with the aspect ratio of the screen from the middle (the
    // cross products compare the ratios without a division), then scale it
    const auto w = static_cast<long>(photo.width);
    const auto h = static_cast<long>(photo.height);
    int part_width = photo.width;
    int part_height = photo.height;
    if (w * height > h * width) { // wider than the screen
        part_width = std::max(1, static_cast<int>(h * width / height));
    } else {
        part_height = std::max(1, static_cast<int>(w * height / width));
    }
    const Image part =
            crop(photo, (photo.width - part_width) / 2,
                 (photo.height - part_height) / 2, part_width, part_height);
    auto out = resize_image(part, width, height);
    if (out) {
        for (uint32_t& pixel : out->pixels) {
            pixel |= OPAQUE;
        }
    }
    return out;
}

} // namespace tiny::image
