/**
 * @file lexer_layout.c
 * @brief Whitespace, comments and indentation handling of the IFJ26 lexer.
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

#include <stdbool.h>
#include <stdio.h>

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

t_lex_rc lexer_skip_whitespace_and_comments(t_lexer *lx, bool *had_eol) {
    *had_eol = false;

    for (;;) {
        int c = lexer_peekc(lx);

        if (c == ' ' || c == '\t' || c == '\r') {
            lexer_getc(lx);
        } else if (c == '/' && lexer_peekc2(lx) == '/') {
            lexer_skip_to_eol(lx); // the '\n' stays for the newline logic
        } else if (c == '/' && lexer_peekc2(lx) == '*') {
            int start_line = lx->line;
            int start_col = lx->column;
            lexer_getc(lx); // '/'
            lexer_getc(lx); // '*'

            int depth = 1;
            while (depth > 0) {
                int d = lexer_getc(lx);
                if (d == EOF) {
                    return lexer_error(lx,
                        "unterminated block comment (started at %d:%d)",
                        start_line, start_col);
                }
                if (d == '\n') {
                    *had_eol = true;
                } else if (d == '/' && lexer_peekc(lx) == '*') {
                    lexer_getc(lx);
                    depth++;
                } else if (d == '*' && lexer_peekc(lx) == '/') {
                    lexer_getc(lx);
                    depth--;
                }
            }
        } else {
            return LEX_OK; // '\n', EOF, or the start of a real token
        }
    }
}

t_lex_rc lexer_handle_indentation(t_lexer *lx) {
    int n;

    // Loop: blank and comment-only lines are skipped and counting restarts.
    for (;;) {
        // 1. count (and consume) the spaces at the start of the line
        n = 0;
        while (lexer_peekc(lx) == ' ') {
            lexer_getc(lx);
            n++;
        }

        bool saw_tab = (lexer_peekc(lx) == '\t');

        // 2. skip comments and further whitespace, then look at what is left
        bool had_eol;
        LEX_TRY(lexer_skip_whitespace_and_comments(lx, &had_eol));
        int c = lexer_peekc(lx);

        if (c == '\n') { // blank or comment-only line
            lexer_getc(lx);
            continue;
        }
        if (c == EOF) { // lexer_next closes the open blocks
            lx->at_line_start = false;
            return LEX_OK;
        }
        if (saw_tab) { // a tab only matters on a line that has real content
            return lexer_error(lx, "tab used for indentation");
        }
        if (had_eol) {
            return lexer_error(lx,
                "token after a multi-line comment on the same line");
        }
        break; // a real line, `n` is its indentation
    }

    // 3. compare with the indent stack
    int top = lx->indent_stack[lx->indent_top];
    if (n > top) {
        if (lx->indent_top + 1 >= LEXER_MAX_INDENT) {
            return lexer_error(lx, "indentation is nested too deeply");
        }
        lx->indent_stack[++lx->indent_top] = n;
        lx->pending_indent = true;
    } else if (n < top) {
        while (lx->indent_stack[lx->indent_top] > n) {
            lx->indent_top--;
            lx->pending_dedents++;
        }
        if (lx->indent_stack[lx->indent_top] != n) {
            return lexer_error(lx, "inconsistent indentation");
        }
    }

    lx->at_line_start = false;
    return LEX_OK;
}