#!/usr/bin/env python3

from encode_frames import encode_rle

def test_rle_encoding() -> None:
    with open('build/test_rle_encoding.c', 'w') as fp:
        fp.write(
r'''
#include <bad-apple.h>
#include <stdio.h>

struct TestEncodeRLE {
    const uint8_t *data;
    size_t data_size;
    uint32_t expected;
};

const struct TestEncodeRLE tests[] = {
''')

        buf = bytearray()
        max_length = 480 * 360
        for length in range(1, max_length + 1):
            buf.clear()
            encode_rle(0, length, buf)

            fmt_buf = ', '.join(f'0x{b:02x}' for b in buf)
            fp.write(
f'''\
    {{ .data = (uint8_t[]){{ {fmt_buf} }}, .data_size = {len(buf)}, .expected = {length} }},
''')

        fp.write(f'''\
}};

const size_t test_count = {max_length - 1};
''')

        fp.write(
r'''
int main() {
    size_t error_count = 0;
    for (size_t index = 0; index < test_count; ++ index) {
        const struct TestEncodeRLE *test = &tests[index];
        struct ComprCmd cmd;
        size_t compr_len = compr_cmd_decode(test->data, test->data_size, &cmd);
        if (compr_len != test->data_size) {
            fprintf(stderr,
                "[%u] compr_len != test->data_size: %zu != %zu\n",
                test->expected, compr_len, test->data_size);
            ++ error_count;
            continue;
        }

        if (cmd.type != 0) {
            fprintf(stderr,
                "[%u] type != 0: %u != 0\n",
                test->expected, cmd.type);
            ++ error_count;
            continue;
        }

        if (cmd.length != test->expected) {
            fprintf(stderr, "[%d] {", test->expected);
            for (size_t byte_index = 0; byte_index < test->data_size; ++ byte_index) {
                uint8_t byte = test->data[byte_index];
                if (byte_index > 0) {
                    fprintf(stderr, ", 0x%02x", (unsigned int)byte);
                } else {
                    fprintf(stderr, "0x%02x", (unsigned int)byte);
                }
            }

            fprintf(stderr,
                "} cmd.length != test->expected: %u != %u\n",
                cmd.length, test->expected);
            ++ error_count;
            continue;
        }
    }

    fprintf(stderr, "tests: %zu, success: %zu, failed: %zu\n",
        test_count, test_count - error_count, error_count);

    return error_count > 0 ? 1 : 0;
}
''')

if __name__ == '__main__':
    test_rle_encoding()
