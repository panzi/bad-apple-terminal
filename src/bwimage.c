#include "bad-apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct BWImage bwimage_new(int32_t width, int32_t height) {
    size_t size = bwimage_nbytes(width, height);

    return (struct BWImage){
        .width = width,
        .height = height,
        .data = calloc(size, 1),
    };
}

void bwimage_free(struct BWImage *image) {
    free(image->data);
    image->data = NULL;
    image->width = 0;
    image->height = 0;
}

void bwimage_copy_from(struct BWImage *dest, const struct BWImage *src) {
    assert(dest->width == src->width);
    assert(dest->height == src->height);

    memcpy(dest->data, src->data, bwimage_nbytes(dest->width, dest->height));
}

size_t compr_cmd_decode(const uint8_t *data, size_t size, struct ComprCmd *cmd) {
    if (size == 0) {
        return 0;
    }

    uint8_t byte = data[0];
    enum ComprCmdType type = byte >> 6;
    uint32_t length = byte & 0x1F;

    size_t index = 1;
    if (byte & 0x20) {
        uint32_t shift = 5;
        do {
            if (index >= size) {
#ifndef NDEBUG
                fprintf(stderr, "compr_cmd_decode(): index out of bounds: %zu >= %zu\n", index, size);
#endif
                assert(false); // index out of bounds
                return index;
            }
            assert(index <= 2);
            byte = data[index];
            length += ((uint32_t)(byte & 0x7F) + 1) << shift;
            shift += 7;
            index += 1;
        } while (byte & 0x80);
    }

    length += 1;

    if (cmd != NULL) {
        cmd->type = type;
        cmd->length = length;
    }

    return index;
}

static inline void bwimage_set_color_rle(uint8_t *data, size_t pixel_index, size_t pixel_end_index, uint8_t value) {
    size_t byte_index = pixel_index >> 3;
    size_t bit_index = pixel_index & 7;

    size_t byte_end_index = pixel_end_index >> 3;
    size_t bit_end_index = pixel_end_index & 7;

    if (byte_index == byte_end_index) {
        uint8_t byte = data[byte_index];
        uint8_t bit_mask = (0xFF >> bit_index) & ~(0xFF >> bit_end_index);
        data[byte_index] = (byte & ~bit_mask) | (value & bit_mask);
    } else {
        if (bit_index) {
            uint8_t byte = data[byte_index];
            uint8_t bit_mask = 0xFF >> bit_index;
            data[byte_index] = (byte & ~bit_mask) | (value & bit_mask);
            byte_index += 1;
        }

        if (byte_end_index > byte_index) {
            memset(data + byte_index, value, byte_end_index - byte_index);
        }

        if (bit_end_index) {
            uint8_t byte = data[byte_end_index];
            uint8_t bit_mask = 0xFF >> bit_end_index;
            data[byte_end_index] = (byte & bit_mask) | (value & ~bit_mask);
        }
    }
}

bool bwimage_decompress(const struct BWImage *prev_frame, const struct CompressedFrame *compressed, struct BWImage *frame) {
    bwimage_copy_from(frame, prev_frame);

    size_t compr_size = compressed->size;
    const uint8_t *compr_data = compressed->data;

    size_t pixel_index = 0;
    size_t pixel_size = frame->width * frame->height;
    uint8_t *frame_data = frame->data;
    for (size_t compr_index = 0; compr_index < compr_size;) {
        struct ComprCmd cmd;
        size_t rem_compr_size = compr_size - compr_index;
        size_t compr_len = compr_cmd_decode(compr_data + compr_index, rem_compr_size, &cmd);
        if (compr_len > rem_compr_size) {
            return false;
        }

        size_t pixel_end_index = pixel_index + cmd.length;
        if (pixel_end_index > pixel_size) {
#ifndef NDEBUG
            fprintf(stderr, "bwimage_decompress(): pixel index oud of bounds: %zu > %zu\n", pixel_end_index, pixel_size);
#endif
            return false;
        }

        switch (cmd.type) {
            case ComprCmd_Skip:
                break;

            case ComprCmd_White:
                // set pixels white
                bwimage_set_color_rle(frame_data, pixel_index, pixel_end_index, 0xFF);
                break;

            case ComprCmd_Black:
                // set pixels black
                bwimage_set_color_rle(frame_data, pixel_index, pixel_end_index, 0x00);
                break;

            case ComprCmd_Flip:
            {
                // flip pixels
                size_t byte_index = pixel_index >> 3;
                size_t bit_index = pixel_index & 7;

                size_t byte_end_index = pixel_end_index >> 3;
                size_t bit_end_index = pixel_end_index & 7;

                if (byte_index == byte_end_index) {
                    uint8_t byte = frame_data[byte_index];
                    uint8_t bit_mask = (0xFF >> bit_index) & ~(0xFF >> bit_end_index);
                    frame_data[byte_index] = (byte & ~bit_mask) | (~byte & bit_mask);
                } else {
                    if (bit_index) {
                        uint8_t byte = frame_data[byte_index];
                        uint8_t bit_mask = 0xFF >> bit_index;
                        frame_data[byte_index] = (byte & ~bit_mask) | (~byte & bit_mask);
                        byte_index += 1;
                    }

                    for (size_t index = byte_index; index < byte_end_index; ++ index) {
                        frame_data[index] = ~frame_data[index];
                    }

                    if (bit_end_index) {
                        uint8_t byte = frame_data[byte_end_index];
                        uint8_t bit_mask = 0xFF >> bit_end_index;
                        frame_data[byte_end_index] = (byte & bit_mask) | (~byte & ~bit_mask);
                    }
                }
                break;
            }
            default:
                assert(0);
        }

        pixel_index = pixel_end_index;
        compr_index += compr_len;
    }

    return true;
}

static const char *bwimage_patterns[] = {
    " ", "🬀", "🬁", "🬂", "🬃", "🬄", "🬅", "🬆", "🬇", "🬈", "🬉", "🬊", "🬋", "🬌", "🬍",
    "🬎", "🬏", "🬐", "🬑", "🬒", "🬓", "▌", "🬔", "🬕", "🬖", "🬗", "🬘", "🬙", "🬚", "🬛",
    "🬜", "🬝", "🬞", "🬟", "🬠", "🬡", "🬢", "🬣", "🬤", "🬥", "🬦", "🬧", "▐", "🬨", "🬩",
    "🬪", "🬫", "🬬", "🬭", "🬮", "🬯", "🬰", "🬱", "🬲", "🬳", "🬴", "🬵", "🬶", "🬷", "🬸",
    "🬹", "🬺", "🬻", "█",
};

void bwimage_render_ansi_diff(const struct BWImage *prev_frame, const struct BWImage *frame) {
    (void)bwimage_patterns;

    uint32_t width = frame->width;
    uint32_t height = frame->height;
    for (uint32_t y = 0; y < height; y += 3) {
        for (uint32_t x = 0; x < width; x += 2) {
        }
    }
    fprintf(stderr, "bwimage_render_ansi_diff(): not implemented\n");
    assert(0); // not implemented
    exit(1);
}

// bit layout
// 1 2
// 3 4
// 5 6

#if 1
void bwimage_render_ansi_full(const struct BWImage *frame) {
    uint32_t width = frame->width;
    uint32_t height = frame->height;
    unsigned int line_len = (width + 1) / 2;

    printf("\x1B[38;2;255;255;255m\x1B[48;2;0;0;0m");
    for (uint32_t y = 0; y < height; y += 3) {
        uint32_t y2 = y + 1;
        uint32_t y3 = y + 2;

        if (y > 0) {
            printf("\x1B[%uD\x1B[1B", line_len);
        }
        for (uint32_t x = 0; x < width; x += 2) {
            uint32_t x2 = x + 1;
            uint32_t pattern_bits = (uint32_t)bwimage_get_pixel(frame, x, y);

            if (x2 < width) {
                pattern_bits |= (uint32_t)bwimage_get_pixel(frame, x2, y) << 1;
            }

            if (y2 < height) {
                pattern_bits |= (uint32_t)bwimage_get_pixel(frame, x, y2) << 2;

                if (x2 < width) {
                    pattern_bits |= (uint32_t)bwimage_get_pixel(frame, x2, y2) << 3;
                }

                if (y3 < height) {
                    pattern_bits |= (uint32_t)bwimage_get_pixel(frame, x, y3) << 4;

                    if (x2 < width) {
                        pattern_bits |= (uint32_t)bwimage_get_pixel(frame, x2, y3) << 5;
                    }
                }
            }

            const char *pattern = bwimage_patterns[(size_t)pattern_bits];

            fwrite(pattern, 1, strlen(pattern), stdout);
        }
    }
    printf("\x1B[0m");
}
#else
void bwimage_render_ansi_full(const struct BWImage *frame) {
    uint32_t width = frame->width;
    uint32_t height = frame->height;
    unsigned int line_len = (unsigned int)width * 2;

    printf("\x1B[38;2;255;255;255m\x1B[48;2;0;0;0m");
    for (uint32_t y = 0; y < height; ++ y) {
        if (y > 0) {
            printf("\x1B[%uD\x1B[1B", line_len);
        }
        for (uint32_t x = 0; x < width; ++ x) {
            if (bwimage_get_pixel(frame, x, y)) {
                printf("██");
            } else {
                printf("  ");
            }
        }
    }
    printf("\x1B[0m");
}
#endif
