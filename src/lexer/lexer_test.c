#include <stdio.h>
#include "lexer.h"

static void print_str(const char *s) {
    for (; *s; s++) {
        if (*s == '\n') printf("\\n");
        else if (*s == '\r') printf("\\r");
        else if (*s == '\t') printf("\\t");
        else if (*s == '\\') printf("\\\\");
        else putchar(*s);
    }
}

int main(void) {
    t_lexer lx;
    t_token tok;
    token_init(&tok);
    if (lexer_init(&lx, stdin) != LEX_OK) return 99;

    t_lex_rc rc;
    do {
        rc = lexer_next(&lx, &tok);
        if (rc != LEX_OK) break;

        printf("%s", token_type_name(tok.type));
        if (token_has_string(tok.type)) {
            printf("(");
            print_str(tok.value.str_val);
            printf(")");
        }
        if (tok.type == TOKEN_INT_LIT) printf("(%lld)", tok.value.int_val);
        if (tok.type == TOKEN_DOUBLE_LIT) printf("(%g)", tok.value.double_val);
        printf(tok.type == TOKEN_EOL || tok.type == TOKEN_EOF ? "\n" : " ");
    } while (tok.type != TOKEN_EOF);

    token_free(&tok);
    lexer_destroy(&lx);
    return rc;
}