// stb_image again, for photos: PNG and JPEG from memory. The functions are
// static here, so they do not clash with the ones of vendor.cc (PNG only,
// for icons) and only the programs that decode a photo get the JPEG code.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_MAX_DIMENSIONS 16384
#include <stb_image.h>

#include "third_party/vendor_photo.h"

unsigned char* photo_decode(const unsigned char* data, int size,
                            long max_pixels, int* width, int* height)
{
    int channels = 0;
    if (!stbi_info_from_memory(data, size, width, height, &channels) ||
        static_cast<long>(*width) * *height > max_pixels) {
        return nullptr;
    }
    return stbi_load_from_memory(data, size, width, height, &channels, 4);
}

void photo_free(unsigned char* rgba)
{
    stbi_image_free(rgba);
}
