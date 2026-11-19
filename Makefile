CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -O0
CPPFLAGS = -Isrc

TARGET = ifj26compiler

SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(TARGET)
