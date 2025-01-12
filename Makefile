CC = gcc
CFLAGS = -Wall -std=gnu2x -Werror -fvisibility=hidden
BUILD_PREFIX = build
OBJ = $(BUILD_DIR)/main.o $(BUILD_DIR)/frames.o $(BUILD_DIR)/bwimage.o
BIN = $(BUILD_DIR)/bad-apple
DEBUG = ON
AR = ar
PREFIX = /usr/local

ifeq ($(DEBUG),ON)
	CFLAGS += -g
	BUILD_DIR = $(BUILD_PREFIX)/debug
else
	CFLAGS += -O3 -DNDEBUG
	BUILD_DIR = $(BUILD_PREFIX)/release
endif

.PHONY: all clean frames run clean-all test-rle-encoding

all: $(BIN)

frames: $(BUILD_PREFIX)/frames/frame0001.png

run: $(BIN)
	$(BIN)

test-rle-encoding: $(BUILD_DIR)/test_rle_encoding
	$(BUILD_DIR)/test_rle_encoding

$(BUILD_DIR)/test_rle_encoding: $(BUILD_DIR)/test_rle_encoding.o $(BUILD_DIR)/bwimage.o
	$(CC) $(CFLAGS) -o $@ $^

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/test_rle_encoding.o: $(BUILD_PREFIX)/test_rle_encoding.c src/bad-apple.h
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD_PREFIX)/test_rle_encoding.c: test_rle_encoding.py
	./test_rle_encoding.py

$(BUILD_DIR)/frames.o: $(BUILD_PREFIX)/frames.c src/bad-apple.h
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.c src/bad-apple.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_PREFIX)/frames.c: $(BUILD_PREFIX)/frames/frame0001.png encode_frames.py
	./encode_frames.py

$(BUILD_PREFIX)/frames/frame%.png: $(BUILD_PREFIX)/bad-apple.webm
	mkdir -p $(BUILD_PREFIX)/frames
	ffmpeg -i $(BUILD_PREFIX)/bad-apple.webm $(BUILD_PREFIX)/frames/frame%04d.png

$(BUILD_PREFIX)/bad-apple.webm:
	yt-dlp "https://www.youtube.com/watch?v=FtutLA63Cp8" --output build/bad-apple.webm

clean:
	rm -v $(OBJ) $(BIN) $(BUILD_DIR)/test_rle_encoding.o

clean-all:
	rm -v $(OBJ) $(BIN) \
		$(BUILD_PREFIX)/test_rle_encoding.c \
		$(BUILD_PREFIX)/frames.c \
		$(BUILD_PREFIX)/bad-apple.webm \
		$(wildcard $(BUILD_PREFIX)/frames/frame*.png)
