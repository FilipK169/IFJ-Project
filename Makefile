# ========================================== #
# ================ CONFIG ================== #
# ========================================== #

CC = gcc
AR = ar

CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -O0
CPPFLAGS = -I.

TARGET = libsemantics.a

SOURCES = bst.c \
          symtable.c \
          semantics.c \
          ast.c

OBJECTS = $(SOURCES:.c=.o)
DEPS = $(OBJECTS:.o=.d)

TEST_TARGET = test_semantics

.PHONY: all clean rebuild test

test: $(TARGET) test_semantics.c
	$(CC) $(CPPFLAGS) $(CFLAGS) test_semantics.c $(TARGET) -o $(TEST_TARGET)
	./$(TEST_TARGET)

# ========================================== #
# ================ TARGETS ================= #
# ========================================== #

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET) $(TEST_TARGET)

rebuild: clean all

-include $(DEPS)