#include "image/image.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string_view>

#include <stb_image.h>

#include <nanosvg.h>
#include <nanosvgrast.h>

#include "image/xpm.h"

namespace tiny::image {

namespace {

// the biggest text files: a 512 x 512 XPM with two characters per pixel has
// 0.5 MB, the SVG of an icon is mostly under 100 kB
constexpr std::uintmax_t MAX_XPM_FILE = 2 << 20;
constexpr std::uintmax_t MAX_SVG_FILE = 4 << 20;

// The size of the file when it is a regular file of a sane size.
std::optional<size_t> file_size_up_to(const std::string& path,
                                      std::uintmax_t limit)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > limit) {
        return std::nullopt;
    }
    return static_cast<size_t>(size);
}

bool has_extension(const std::string& path, const char* extension)
{
    const std::string_view view(extension);
    if (path.size() < view.size()) {
        return false;
    }
    return std::equal(
            view.rbegin(), view.rend(), path.rbegin(), [](char want, char got) {
                return want == std::tolower(static_cast<unsigned char>(got));
            });
}

// non-premultiplied RGBA bytes -> premultiplied ARGB
Image convert(const unsigned char* rgba, int width, int height)
{
    const size_t count =
            static_cast<size_t>(width) * static_cast<size_t>(height);
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const unsigned char* p = rgba + 4 * i;
        const uint32_t a = p[3];
        const auto mul = [a](uint32_t c) { return (c * a + 127) / 255; };
        image.pixels.push_back(a << 24 | mul(p[0]) << 16 | mul(p[1]) << 8 |
                               mul(p[2]));
    }
    return image;
}

std::optional<Image> load_png(const std::string& path)
{
    int width = 0;
    int height = 0;
    // the header says how much memory the picture needs
    if (!stbi_info(path.c_str(), &width, &height, nullptr) ||
        width > MAX_SOURCE_SIZE || height > MAX_SOURCE_SIZE) {
        return std::nullopt;
    }
    const std::unique_ptr<unsigned char, decltype(&stbi_image_free)> data(
            stbi_load(path.c_str(), &width, &height, nullptr, 4),
            &stbi_image_free);
    if (!data) {
        return std::nullopt;
    }
    return convert(data.get(), width, height);
}

std::optional<Image> load_xpm(const std::string& path)
{
    const auto size = file_size_up_to(path, MAX_XPM_FILE);
    std::ifstream file(path, std::ios::binary);
    if (!size || !file) {
        return std::nullopt;
    }
    std::string text(*size, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(file.gcount()));
    return parse_xpm(text);
}

std::optional<Image> load_svg(const std::string& path, int size)
{
    if (!file_size_up_to(path, MAX_SVG_FILE)) {
        return std::nullopt;
    }
    const std::unique_ptr<NSVGimage, decltype(&nsvgDelete)> svg(
            nsvgParseFromFile(path.c_str(), "px", 96), &nsvgDelete);
    if (!svg || !(svg->width > 0) || !(svg->height > 0)) {
        return std::nullopt;
    }

    const float scale =
            static_cast<float>(size) / std::max(svg->width, svg->height);
    const int width = std::max(
            1,
            std::min(size, static_cast<int>(std::lround(svg->width * scale))));
    const int height = std::max(
            1,
            std::min(size, static_cast<int>(std::lround(svg->height * scale))));

    const std::unique_ptr<NSVGrasterizer, decltype(&nsvgDeleteRasterizer)>
            rasterizer(nsvgCreateRasterizer(), &nsvgDeleteRasterizer);
    if (!rasterizer) {
        return std::nullopt;
    }
    std::vector<unsigned char> rgba(static_cast<size_t>(width) *
                                    static_cast<size_t>(height) * 4);
    nsvgRasterize(rasterizer.get(), svg.get(), 0, 0, scale, rgba.data(), width,
                  height, width * 4);
    return convert(rgba.data(), width, height);
}

} // namespace

std::optional<Image> load_image(const std::string& path, int size)
{
    if (size < 1 || size > MAX_ICON_SIZE) {
        return std::nullopt;
    }
    if (has_extension(path, ".png")) {
        return load_png(path);
    }
    if (has_extension(path, ".xpm")) {
        return load_xpm(path);
    }
    if (has_extension(path, ".svg")) {
        return load_svg(path, size);
    }
    return std::nullopt;
}

namespace {

constexpr int CHANNEL_BITS = 8;

// the centers of the pixels are mapped, the edge pixels are repeated
uint32_t bilinear(const Image& image, double fx, double fy)
{
    const double x = std::clamp(fx, 0.0, image.width - 1.0);
    const double y = std::clamp(fy, 0.0, image.height - 1.0);
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, image.width - 1);
    const int y1 = std::min(y0 + 1, image.height - 1);
    const double dx = x - x0;
    const double dy = y - y0;

    uint32_t pixel = 0;
    for (int shift = 0; shift < 32; shift += CHANNEL_BITS) {
        const auto channel = [&](int px, int py) {
            return static_cast<double>((image.at(px, py) >> shift) & 0xFFU);
        };
        const double top = channel(x0, y0) * (1 - dx) + channel(x1, y0) * dx;
        const double bottom = channel(x0, y1) * (1 - dx) + channel(x1, y1) * dx;
        pixel |=
                static_cast<uint32_t>(std::lround(top * (1 - dy) + bottom * dy))
                << shift;
    }
    return pixel;
}

// the average of the source area [x0, x1) x [y0, y1), the edge pixels count
// by the part that the area covers
uint32_t average(const Image& image, double x0, double y0, double x1, double y1)
{
    double sum[4] = {};
    double weight = 0;
    const int last_x = std::min(static_cast<int>(std::ceil(x1)), image.width);
    const int last_y = std::min(static_cast<int>(std::ceil(y1)), image.height);
    for (int y = static_cast<int>(y0); y < last_y; ++y) {
        const double wy =
                std::min(y + 1.0, y1) - std::max(static_cast<double>(y), y0);
        for (int x = static_cast<int>(x0); x < last_x; ++x) {
            const double w = wy * (std::min(x + 1.0, x1) -
                                   std::max(static_cast<double>(x), x0));
            const uint32_t pixel = image.at(x, y);
            for (int c = 0; c < 4; ++c) {
                sum[c] += w * ((pixel >> (c * CHANNEL_BITS)) & 0xFFU);
            }
            weight += w;
        }
    }

    uint32_t pixel = 0;
    for (int c = 0; c < 4; ++c) {
        pixel |= static_cast<uint32_t>(std::lround(sum[c] / weight))
                 << (c * CHANNEL_BITS);
    }
    return pixel;
}

} // namespace

std::optional<Image> scale_image(const Image& image, int size)
{
    if (size < 1 || size > MAX_ICON_SIZE || !image.valid()) {
        return std::nullopt;
    }
    if (std::max(image.width, image.height) == size) {
        return image;
    }

    const double ratio =
            static_cast<double>(size) /
            static_cast<double>(std::max(image.width, image.height));
    Image out;
    out.width = std::max(1, static_cast<int>(std::lround(image.width * ratio)));
    out.height =
            std::max(1, static_cast<int>(std::lround(image.height * ratio)));
    out.pixels.resize(static_cast<size_t>(out.width) *
                      static_cast<size_t>(out.height));

    // the pixel of the result covers the source area of the same part of the
    // picture, not of the same size when the rounding changed the ratio
    const double step_x = static_cast<double>(image.width) / out.width;
    const double step_y = static_cast<double>(image.height) / out.height;
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const uint32_t pixel =
                    ratio < 1 ? average(image, x * step_x, y * step_y,
                                        (x + 1) * step_x, (y + 1) * step_y)
                              : bilinear(image, (x + 0.5) / ratio - 0.5,
                                         (y + 0.5) / ratio - 0.5);
            out.pixels[static_cast<size_t>(y) * static_cast<size_t>(out.width) +
                       static_cast<size_t>(x)] = pixel;
        }
    }
    return out;
}

} // namespace tiny::image
