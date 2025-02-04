Bad Apple!! but in the Unix Terminal
====================================

Recently I've been drawing various stuff in the terminal using [ANSI escape
sequences](https://en.wikipedia.org/wiki/ANSI_escape_code) and Unicode. In
particular I drew pixel images using [Block
Elements](https://en.wikipedia.org/wiki/Block_Elements) like `▄`, `▀`, and `█`
to display two pixels per character, coloring them with foreground and
background colors. Or I was using `▏`, `▎`, `▍`, `▌`, `▋`, `▊`, `▉`, and `█` to
draw high resolution progress bars. But then I saw [Nolen
Royalty](https://eieio.games/blog/bad-apple-with-regex-in-vim/) playing [Bad
Apple!!](https://knowyourmeme.com/memes/bad-apple) in vim using regular
expressions! (It's the same Nolen Royalty who made [One Million
Checkboxes](https://eieio.games/blog/one-million-checkboxes/).)

## Run this program

```bash
git clone git@github.com:panzi/bad-apple-terminal.git
cd bad-apple-terminal
make DEBUG=OFF run
```

This needs [yt-dlp](https://github.com/yt-dlp/yt-dlp), to download the original
animation [ffmpeg](https://www.ffmpeg.org/) to convert the video into its
frames, and Python with [Pillow](https://pypi.org/project/pillow/) to generate
the C program, which will then be compiled and executed.

## Rendering Pixels on the Terminal

Since that video is black and white, I thought that is finally the perfect
chance to use the Unicode [Symbols for Legacy
Computing](https://en.wikipedia.org/wiki/Symbols_for_Legacy_Computing). These
allow to display 6 pixels per character:

| | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
| :- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: |
| U+1FB0x | `🬀` | `🬁` | `🬂` | `🬃` | `🬄` | `🬅` | `🬆` | `🬇` | `🬈` | `🬉` | `🬊` | `🬋` | `🬌` | `🬍` | `🬎` | `🬏` |
| U+1FB1x | `🬐` | `🬑` | `🬒` | `🬓` | `🬔` | `🬕` | `🬖` | `🬗` | `🬘` | `🬙` | `🬚` | `🬛` | `🬜` | `🬝` | `🬞` | `🬟` |
| U+1FB2x | `🬠` | `🬡` | `🬢` | `🬣` | `🬤` | `🬥` | `🬦` | `🬧` | `🬨` | `🬩` | `🬪` | `🬫` | `🬬` | `🬭` | `🬮` | `🬯` |
| U+1FB3x | `🬰` | `🬱` | `🬲` | `🬳` | `🬴` | `🬵` | `🬶` | `🬷` | `🬸` | `🬹` | `🬺` | `🬻` | `🬼` | `🬽` | `🬾` | `🬿` |

Those pixels aren't square. The actual form factor depends on your terminal
font, it often even varies between font sizes of the same font. I assumed that
a character has an aspect ratio of 1:2 and squashed the video in height
accordingly.

Symbols that could display 2x8 pixels would be closer to square. There are
[Braille Patterns](https://en.wikipedia.org/wiki/Braille_Patterns) in Unicode
that are basically that: `⠁`, `⠊`, `⣜`, `⣿`, etc. But the downside with those
is that there is a gap between the "pixels" and some fonts also render the
empty "pixels" as empty circles. Side-note: A cool example that uses braille
patterns (and optionally block elements) is [MapSCII](https://github.com/rastapasta/mapscii).

## Compression

I wanted to compile it all into one self contained binary and just for an
extra challenge tried to keep the file size somewhat in check. So I've came up
with a very simple compression scheme of run length encoded 1-bit images.

First there are 4 different operations, encoded in 2 bits:

| Bit 8-7 | |
| :- | :- |
| `00` | Skip: Keep the pixels from the previous frame. |
| `01` | White |
| `10` | Black |
| `11` | Flip: Flip the pixels from the previous frame. |

Followed by a length, that is encoded as a variable length integer a little bit
like UTF-8. In the first byte the 6th bit indicates continuation and in any
further byte the 8th bit does it. The other bits are the payload, but since I
never encode a length of 0 all zeros represent 1. The least significant bits are
stored first, similar to little endian encoding.

| Byte 1     | Byte 2     | Byte 3     |
| :--------- | :--------- | :--------- |
| `xx0yyyyy` |            |            |
| `xx1yyyyy` | `0yyyyyyy` |            |
| `xx1yyyyy` | `1yyyyyyy` | `0yyyyyyy` |

In theory there could be more than 3 bytes, I'm encoding unsigned 32 bit values,
but since the video I used has a resolution of 480x360 the maximum encoded value
is only 172800 and that fits into 3 bytes.

Examples:

| Byte 1     | Byte 2     | Byte 3     | Value    |
| :--------- | :--------- | :--------- | -------: |
| `xx000000` |            |            |      `1` |
| `xx011111` |            |            |     `32` |
| `xx100000` | `00000000` |            |     `33` |
| `xx111111` | `01111111` |            |   `4128` |
| `xx100000` | `10000000` | `00000000` |   `4129` |
| `xx111111` | `11111111` | `01111111` | `528416` |

As code this looks like this:

```Python
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
```

Decoding these lengths gave me a little bit of trouble until I realized that
I have to use addition instead of bit-wise or while decoding because of the + 1
possibly overflowing. In an bit-wise or that overlapping value would be just
lost or in any case not produce the right result.

```C
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
```

## Differential Display Update

Then when rendering the image to the terminal I again compare each new frame
with the previous and only update the characters that need changing. I do that
to reduce the number of characters the terminal has to parse. In both cases
where I scan for differences I do that by simply linearly scanning through the
pixels. I don't do any segmentation or anything like that, meaning this could
all be more efficient. It renders smoothly on my machine, so that's good
enough for me, but the executable is a bit big. A stripped release build is
10 MiB, while the original video, which isn't 1-bit but gray-scale (stored in
an RGB video) and includes the music, is only 6.7 MiB.

I suspect any further compression schemes would be significantly more complex.
Maybe just using zlib would compress it well, but I wanted to do this as an
executable without any dependencies. The RGB PNG frames of the video are 138
MiB, so at least my file is smaller than that.

## The Result

Anyway, here is a video of the result with me zooming in and out, showing how
it responds to that:

[![Video of this thing running in a terminal](https://i3.ytimg.com/vi/WerXbHt62E8/maxresdefault.jpg)](https://www.youtube.com/watch?v=WerXbHt62E8)

I used [Alacritty](https://alacritty.org/) for this video. Normally I use
[Konsole](https://konsole.kde.org/), and this of course also runs in that, but
Konsole doesn't allow you to zoom out far enough so you can see the whole
picture.

It also kinda works on the Linux TTY on a 4k monitor using a small font, here
with the `devault8x16` font:

[![Video of this thing running in the Linux TTY](https://i3.ytimg.com/vi/XCI_WkTi5vc/maxresdefault.jpg)](https://www.youtube.com/watch?v=XCI_WkTi5vc)

Sadly there are not TTY console fonts on my machine that support the needed
symbols for legacy computing. But even so it's only weird edges and still
works otherwise.

## Related Projects

I've mentioned other things that I did with Unicode on the terminal lately.
In case you're interested, here is a list of those things:

- [Progress Pride Bar](https://github.com/panzi/progress-pride-bar) (Rust): A
  progress bar for the terminal that looks like the progress pride flag.
- [Term Flags](https://github.com/panzi/python-term-flags) (Python): A primitive
  sytem to render simple scalable flags on the terminal using Unicode.
- [Color Cycling](https://github.com/panzi/rust-color-cycle) (Rust): This is a
  method to give otherwise static pixel art images some kind of animation using
  its color palette.
- [ANSI IMG](https://github.com/panzi/ansi-img) (Rust): Display images (including
  animated GIFs) on the terminal.
- [Unicode Bar Charts](https://github.com/panzi/js-unicode-bar-chart)
  (JavaScript): Draw bar charts on the terminal. With 8 steps per character and
  with colors.
- [Unicode Progress Bars](https://github.com/panzi/js-unicode-progress-bar)
  (JavaScript): Draw bar charts on the terminal. With 8 steps per character,
  border styles, and colors.
- [Unicode Unicode Plots](https://github.com/panzi/js-unicode-plot) (JavaScript):
  Very simple plotting on the terminal. No colors.

