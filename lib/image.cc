#include "image.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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

constexpr int MAX_SIZE = 4096;
constexpr std::streamsize MAX_FILE = 16 << 20; // of a text picture

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
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::string text(static_cast<size_t>(MAX_FILE), '\0');
    file.read(text.data(), MAX_FILE);
    text.resize(static_cast<size_t>(file.gcount()));
    if (file.gcount() == MAX_FILE && file.peek() != EOF) {
        return std::nullopt; // too big for an icon
    }
    return parse_xpm(text);
}

std::optional<Image> load_svg(const std::string& path, int size)
{
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
    if (size < 1 || size > MAX_SIZE) {
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
