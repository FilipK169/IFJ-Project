CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
SRC    = token.c lexer.c lexer_chars.c lexer_layout.c lexer_scan.c lexer_string.c lexer_test.c
HDR    = token.h lexer.h lexer_internal.h

lexer_test: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o lexer_test_san $(SRC)
	LEXER_TEST=./lexer_test_san ./run_tests.sh

clean:
	rm -f lexer_test lexer_test_san

.PHONY: test clean