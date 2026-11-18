CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g

lexer_test: token.c lexer.c lexer_test.c
	$(CC) $(CFLAGS) -o lexer_test token.c lexer.c lexer_test.c

clean:
	rm -f lexer_test