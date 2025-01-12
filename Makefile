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

.PHONY: all clean frames run clean-all

all: $(BIN)

frames: $(BUILD_PREFIX)/frames/frame0001.png

run: $(BIN)
	$(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

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
	rm -v $(OBJ) $(BIN)

clean-all:
	rm -v $(OBJ) $(BIN) $(BUILD_PREFIX)/bad-apple.webm $(wildcard $(BUILD_PREFIX)/frames/frame*.png)
