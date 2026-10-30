#include "image.h"

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

#include "xpm.h"

namespace tiny {

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

} // namespace tiny
