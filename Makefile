CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -O0
CPPFLAGS = -Isrc

CLANG_FORMAT ?= clang-format

TARGET = ifj26compiler
ZIP_TARGET = IFJ-Project.zip

SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h)
FORMAT_FILES = $(shell find . -type f \( -name '*.c' -o -name '*.h' \))

.PHONY: all clean zip format

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(TARGET)

zip:
	rm -f $(ZIP_TARGET)
	zip -r $(ZIP_TARGET) src dokumentace.pdf rozdeleni rozsireni Makefile

format:
	$(CLANG_FORMAT) -i --style=file $(FORMAT_FILES)