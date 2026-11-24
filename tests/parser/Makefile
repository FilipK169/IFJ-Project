CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -I../../src

LEXDIR = ../../src
ASTDIR = ../../src
PARSERDIR = ../../src

# lexer_test.c is NOT here on purpose (it has its own main, would clash with parser_test.c)
LEXSRC = $(LEXDIR)/token.c $(LEXDIR)/lexer.c $(LEXDIR)/lexer_chars.c \
         $(LEXDIR)/lexer_layout.c $(LEXDIR)/lexer_scan.c $(LEXDIR)/lexer_string.c
ASTSRC = $(ASTDIR)/ast.c
PARSERSRC = $(PARSERDIR)/parser.c $(PARSERDIR)/parser_expr.c $(PARSERDIR)/parser_stmt.c

SRC = $(PARSERSRC) ./parser_test.c $(LEXSRC) $(ASTSRC)
HDR = $(PARSERDIR)/parser.h $(PARSERDIR)/parser_internal.h \
      $(LEXDIR)/token.h $(LEXDIR)/lexer.h $(LEXDIR)/lexer_internal.h \
      $(ASTDIR)/ast.h

parser_test: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o parser_test_san $(SRC)
	PARSER_TEST=./parser_test_san ./run_tests.sh

clean:
	rm -f parser_test parser_test_san

.PHONY: test clean parser_test