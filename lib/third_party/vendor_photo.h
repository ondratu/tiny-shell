#pragma once

// The decoder of photos (PNG and JPEG only, from memory), see vendor_photo.cc.
// Its own copy of stb_image, so that the programs without photos do not
// carry the JPEG decoder.

/**
 * RGBA bytes of the picture, 4 per pixel, to free with `photo_free`. Null
 * when the data are not a PNG or a JPEG, are broken, or have more than
 * `max_pixels` pixels (known from the header, before the memory is taken).
 */
unsigned char* photo_decode(const unsigned char* data, int size,
                            long max_pixels, int* width, int* height);

void photo_free(unsigned char* rgba);
