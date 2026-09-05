CC := gcc
CFLAGS := -std=c23 -Wall -Wextra -Werror $(shell pkg-config --cflags sdl3)
LDFLAGS := $(shell pkg-config --libs sdl3)

SRC_DIR := src
BUILD_DIR := build
RELEASE_DIR := $(BUILD_DIR)/release
DEBUG_DIR := $(BUILD_DIR)/debug

TARGET := $(RELEASE_DIR)/main
TARGET_DEBUG := $(DEBUG_DIR)/main

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS_RELEASE := $(SRCS:$(SRC_DIR)/%.c=$(RELEASE_DIR)/%.o)
OBJS_DEBUG := $(SRCS:$(SRC_DIR)/%.c=$(DEBUG_DIR)/%.o)

all: $(TARGET)

debug: $(TARGET_DEBUG)

$(TARGET): $(OBJS_RELEASE) | $(RELEASE_DIR)
	$(CC) $(OBJS_RELEASE) -o $@ $(LDFLAGS)

$(TARGET_DEBUG): $(OBJS_DEBUG) | $(DEBUG_DIR)
	$(CC) $(OBJS_DEBUG) -o $@ $(LDFLAGS)

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) -DDEBUG -g -c $< -o $@

$(RELEASE_DIR) $(DEBUG_DIR):
	mkdir -p $@ 

clean: 
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
