#!/usr/bin/env python3

from typing import Optional
import PIL.Image
from PIL.Image import Resampling

# bit 8-7
# 00 ... skip
# 01 ... white
# 10 ... black
# 11 ... flip

# bit pattern                       RLE
# xx000000                   ...      1
# xx011111                   ...     32
# xx100000 00000000          ...     33
# xx111111 01111111          ...   4128
# xx100000 10000000 00000000 ...   4129
# xx111111 11111111 01111111 ... 528416
#
# little endian encoding

def encode_rle(cmd: int, length: int, buf: bytearray) -> None:
    if length == 0:
        return

    length -= 1
    byte = (cmd << 6) | (length & 0b1_1111)
    length >>= 5

    if length:
        byte |= 0b10_0000

    buf.append(byte)

    while length:
        length -= 1
        byte = length & 0b111_1111
        length >>= 7

        if length:
            byte |= 0b1000_0000

        buf.append(byte)

def encode_frames() -> None:
    SKIP  = 0b00
    WHITE = 0b01
    BLACK = 0b10
    FLIP  = 0b11

    with open('build/frames.c', 'w') as fp:
        fp.write('''\
#include <bad-apple.h>

const struct CompressedFrame *bad_apple_frames = (struct CompressedFrame[]){
''')
        img = PIL.Image.open(f'build/frames/frame0001.png')
        width, height = img.size

        # Assume characters in the TTY have an aspect ratio of 1:2.
        # I render 2x3 pixels per character. Meaning there are 3 pixels in the
        # height of a character, but it should be 4. So we need to squish those
        # 4 into the 3.
        new_width = width
        new_height = round(height * 3 / 4)
        new_size = new_width, new_height

        #width = 480
        #height = 360
        fps = 30.0003
        frame_count = 6572
        #frame_count = 256
        prev_frame: Optional[list[bool]] = None
        frame_len = new_width * new_height
        for nr in range(1, frame_count + 1):
            print(f"frame {nr}")
            filename = f'build/frames/frame{nr:04d}.png'
            img = PIL.Image.open(filename)
            frame_width, frame_height = img.size
            if frame_width != width or frame_height != height:
                raise ValueError(f"frame_width: {frame_width}, frame_height: {frame_height} != width: {width}, height: {height}")

            img = img.resize(new_size, Resampling.LANCZOS)
            frame: list[bool] = [
                value >= 64
                for value in img.convert('L').getdata()
            ]

            frame_bytes = bytearray()
            if prev_frame is None:
                index = 0
                while index < frame_len:
                    pixel = frame[index]
                    length = 1
                    while (i := index + length) < frame_len and frame[i] == pixel:
                        length += 1
                    if pixel:
                        encode_rle(WHITE, length, frame_bytes)
                    else:
                        encode_rle(BLACK, length, frame_bytes)
                    index += length
            else:
                index = 0
                while index < frame_len:
                    pixel = frame[index]
                    prev_pixel = prev_frame[index]
                    repeat_len = 1
                    while (i := index + repeat_len) < frame_len and frame[i] == pixel:
                        repeat_len += 1

                    if pixel == prev_pixel:
                        skip_len = 1
                        while (i := index + skip_len) < frame_len and frame[i] == prev_frame[i]:
                            skip_len += 1

                        if repeat_len > skip_len:
                            if pixel:
                                encode_rle(WHITE, repeat_len, frame_bytes)
                            else:
                                encode_rle(BLACK, repeat_len, frame_bytes)
                            index += repeat_len
                        else:
                            if index + skip_len < frame_len:
                                encode_rle(SKIP, skip_len, frame_bytes)
                            index += skip_len

                    else:
                        flip_len = 1
                        while (i := index + flip_len) < frame_len and frame[i] != prev_frame[i]:
                            flip_len += 1

                        if repeat_len >= flip_len:
                            if pixel:
                                encode_rle(WHITE, repeat_len, frame_bytes)
                            else:
                                encode_rle(BLACK, repeat_len, frame_bytes)
                            index += repeat_len
                        else:
                            encode_rle(FLIP, flip_len, frame_bytes)
                            index += flip_len
            prev_frame = frame

            fmt_frame_bytes = ', '.join(f'0x{byte:02x}' for byte in frame_bytes)
            fp.write(f'''\
    {{ .size = {len(frame_bytes)}, .data = (uint8_t[]){{ {fmt_frame_bytes} }} }},
''')

        fp.write(f'''\
}};
const size_t bad_apple_frame_count = {frame_count};
const uint32_t bad_apple_width = {new_width};
const uint32_t bad_apple_height = {new_height};
const double bad_apple_fps = {fps};
''')

if __name__ == '__main__':
    encode_frames()
