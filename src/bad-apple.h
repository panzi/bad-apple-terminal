#ifndef BAD_APPLE_H
#define BAD_APPLE_H
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CompressedFrame {
    size_t size;
    const uint8_t *data;
};

struct BWImage {
    uint32_t width;
    uint32_t height;
    uint8_t *data;
};

extern const struct CompressedFrame *bad_apple_frames;
extern const size_t bad_apple_frame_count;
extern const uint32_t bad_apple_width;
extern const uint32_t bad_apple_height;
extern const double bad_apple_fps;

struct BWImage bwimage_new(int32_t width, int32_t height);
void bwimage_free(struct BWImage *image);
void bwimage_copy_from(struct BWImage *image, const struct BWImage *other);

static inline bool bwimage_get_pixel(const struct BWImage *image, uint32_t x, uint32_t y) {
    assert((x) < image->width && (y) < image->height);

    size_t bit_index = (size_t)y * (size_t)image->width + (size_t)x;
    size_t bit_mask = 1 << (bit_index & 7);
    size_t byte_index = (bit_index >> 3) + (bit_index != 0);
    return (image->data[byte_index] & bit_mask) != 0;
}

bool bwimage_decompress(const struct BWImage *prev_frame, const struct CompressedFrame *compressed, struct BWImage *frame);
void bwimage_render_ansi_diff(const struct BWImage *prev_frame, const struct BWImage *frame);
void bwimage_render_ansi_full(const struct BWImage *frame);

#define bwimage_nbytes(width, height) (((size_t)(width) * (size_t)(height) + 7) / 8)

enum ComprCmdType {
    ComprCmd_Skip  = 0,
    ComprCmd_White = 1,
    ComprCmd_Black = 2,
    ComprCmd_Flip  = 3,
};

struct ComprCmd {
    enum ComprCmdType type;
    uint32_t length;
};

size_t compr_cmd_decode(const uint8_t *data, size_t size, struct ComprCmd *cmd);

#ifdef __cplusplus
}
#endif

#endif
