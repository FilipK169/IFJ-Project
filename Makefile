CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -O0
CPPFLAGS = -Isrc

TARGET = ifj26compiler
ZIP_TARGET = IFJ-Project.zip

SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h)

.PHONY: all clean zip

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(TARGET)

zip:
	rm -f $(ZIP_TARGET)
	zip -r $(ZIP_TARGET) src dokumentace.pdf rozdeleni rozsireni Makefile
