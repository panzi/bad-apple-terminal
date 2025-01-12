#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <signal.h>

#include "bad-apple.h"

static void reset_term() {
    struct termios ttystate;
    int res = tcgetattr(STDIN_FILENO, &ttystate);
    if (res == 0) {
        // turn on canonical mode
        ttystate.c_lflag |= ICANON | ECHO;

        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    }

    // CSI 0 m        Reset or normal, all attributes become turned off
    // CSI ?  7 h     Auto-Wrap Mode (DECAWM), VT100
    // CSI ? 25 h     Show cursor (DECTCEM), VT220
    printf("\x1B[0m\x1B[?25h\x1B[?7h\n");
}

static void singal_handler(int sig) {
    reset_term();
}

int main(int argc, char *argv[]) {
    int status = 0;

    fprintf(stderr, "bad_apple_frames: 0x%zx\n", (uintptr_t)bad_apple_frames);
    fprintf(stderr, "bad_apple_width: %u\n", bad_apple_width);
    fprintf(stderr, "bad_apple_height: %u\n", bad_apple_height);
    fprintf(stderr, "bad_apple_fps: %lf\n", bad_apple_fps);

    struct BWImage frame1 = bwimage_new(bad_apple_width, bad_apple_height);
    struct BWImage frame2 = bwimage_new(bad_apple_width, bad_apple_height);

    if (frame1.data == NULL || frame2.data == NULL) {
        perror("bwimage_new(bad_apple_width, bad_apple_height)");
        goto error;
    }

    struct termios ttystate;
    int res = tcgetattr(STDIN_FILENO, &ttystate);
    if (res == -1) {
        perror("tcgetattr(STDIN_FILENO, &ttystate)");
        goto error;
    }

    // turn off canonical mode
    ttystate.c_lflag &= ~(ICANON | ECHO);

    // minimum of number input read.
    ttystate.c_cc[VMIN] = 0;
    ttystate.c_cc[VTIME] = 0;

    res = tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    if (res == -1) {
        perror("tcsetattr(STDIN_FILENO, TCSANOW, &ttystate)");
        goto error;
    }

    void (*sig_res)(int) = signal(SIGINT, singal_handler);
    if (sig_res == SIG_ERR) {
        perror("signal(SIGINT, singal_handler)");
        goto error;
    }

    // CSI ?  7 l     No Auto-Wrap Mode (DECAWM), VT100.
    // CSI ? 25 l     Hide cursor (DECTCEM), VT220
    // CSI 2 J        Clear entire screen
    printf("\x1B[?25l\x1B[?7l\x1B[2J");

    // animation loop
    struct BWImage *prev_frame = &frame1;
    struct BWImage *current_frame = &frame2;

    bool full_frame = true;
    double frame_duration_sec = 1.0 / bad_apple_fps;
    struct timespec frame_duration = {
        .tv_sec = (time_t)frame_duration_sec,
        .tv_nsec = 0,
    };
    frame_duration.tv_nsec = (time_t)((frame_duration_sec - (double)frame_duration.tv_sec) * 1e9);

    for (size_t frame_index = 0; frame_index < bad_apple_frame_count; ++ frame_index) {
        const struct CompressedFrame *compr_frame = &bad_apple_frames[frame_index];
        if (!bwimage_decompress(prev_frame, compr_frame, current_frame)) {
            goto error;
        }

        // TODO: center and crop image
        printf("\x1B[1;1H");

        if (full_frame || true) {
            bwimage_render_ansi_full(current_frame);
            full_frame = false;
        } else {
            // TODO
            bwimage_render_ansi_diff(prev_frame, current_frame);
        }

        fflush(stdout);

        struct BWImage *tmp_frame = prev_frame;
        prev_frame = current_frame;
        current_frame = tmp_frame;

        // TODO: measure time for more precise frame timings
        if (nanosleep(&frame_duration, NULL) != 0) {
            perror("nanosleep(&frame_duration, NULL)");
            goto error;
        }
    }

    goto cleanup;

error:
    status = 1;

cleanup:
    bwimage_free(&frame1);
    bwimage_free(&frame2);

    reset_term();

    return status;
}
