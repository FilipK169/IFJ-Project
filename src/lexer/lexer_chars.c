/**
 * @file lexer_chars.c
 * @brief Low-level lexer helpers: string buffer, error reporting, character
 *        look-ahead and the mode stack.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "lexer.h"
#include "lexer_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
// ===== PRIVATE CONSTANTS AND TYPEDEFS ===== //
// ========================================== //

#define STRBUF_INITIAL_CAP 16

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/*
 * AI use
 * Prompt: error function with a varaible amount of arguments that prints the error location and message
 */
t_lex_rc lexer_error(t_lexer *lx, const char *fmt, ...) {
    fprintf(stderr, "Lexical error at %d:%d: ", lx->line, lx->column);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
    return LEX_ERR_LEXICAL;
}
/*
 * END AI
 */

// ---------- string buffer ---------- //

t_lex_rc strbuf_init(t_strbuf *sb) {
    sb->data = malloc(STRBUF_INITIAL_CAP);
    if (sb->data == NULL) {
        sb->len = 0;
        sb->cap = 0;
        return LEX_ERR_INTERNAL;
    }
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = STRBUF_INITIAL_CAP;
    return LEX_OK;
}

void strbuf_clear(t_strbuf *sb) {
    sb->len = 0;
    sb->data[0] = '\0';
}

t_lex_rc strbuf_append_char(t_strbuf *sb, char c) {
    // Need room for the new char AND the trailing '\0'.
    if (sb->len + 1 >= sb->cap) {
        size_t new_cap = sb->cap * 2;
        char *new_data = realloc(sb->data, new_cap);
        if (new_data == NULL) {
            return LEX_ERR_INTERNAL; // sb->data is still valid, nothing is lost
        }
        sb->data = new_data;
        sb->cap = new_cap;
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
    return LEX_OK;
}

char *strbuf_take(t_strbuf *sb) {
    char *copy = malloc(sb->len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, sb->data, sb->len + 1); // +1 copies the '\0' too
    strbuf_clear(sb);
    return copy;
}

void strbuf_free(t_strbuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

// ---------- characters ---------- //

int lexer_getc(t_lexer *lx) {
    int c;
    if (lx->peeked != LEX_EMPTY) {
        c = lx->peeked;           // take the char we already looked at
        lx->peeked = lx->peeked2; // shift the second look-ahead forward
        lx->peeked2 = LEX_EMPTY;
    } else {
        c = fgetc(lx->in);
    }

    if (c == '\n') {
        lx->line++;
        lx->column = 1;
    } else if (c != EOF) {
        lx->column++;
    }
    return c;
}

int lexer_peekc(t_lexer *lx) {
    if (lx->peeked == LEX_EMPTY) {
        lx->peeked = fgetc(lx->in);
    }
    return lx->peeked;
}

int lexer_peekc2(t_lexer *lx) {
    if (lexer_peekc(lx) == EOF) { // also fills lx->peeked
        return EOF;
    }
    if (lx->peeked2 == LEX_EMPTY) {
        lx->peeked2 = fgetc(lx->in);
    }
    return lx->peeked2;
}

void lexer_skip_to_eol(t_lexer *lx) {
    int c = lexer_peekc(lx);
    while (c != '\n' && c != EOF) {
        lexer_getc(lx);
        c = lexer_peekc(lx);
    }
    // the '\n' is NOT consumed: the newline logic must still see it
}

// ---------- modes ---------- //

t_lex_rc lexer_push_mode(t_lexer *lx, t_lex_mode mode) {
    if (lx->mode_top + 1 >= LEXER_MAX_MODES) {
        return lexer_error(lx, "string interpolation is nested too deeply");
    }
    lx->modes[++lx->mode_top] = mode;
    return LEX_OK;
}

t_lex_rc lexer_pop_mode(t_lexer *lx) {
    if (lx->mode_top == 0) {
        return LEX_ERR_INTERNAL; // popping the base mode is a bug in our code
    }
    lx->mode_top--;
    return LEX_OK;
}