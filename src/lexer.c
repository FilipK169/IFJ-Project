/**
 * @file lexer.c
 * @brief Public interface of the IFJ26 lexer: init, destroy, mode query and lexer_next.
 *
 * The rest of the lexer lives in lexer_chars.c, lexer_layout.c, lexer_scan.c
 * and lexer_string.c.
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
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

t_lex_rc lexer_init(t_lexer *lx, FILE *in)
{
    lx->in = in;

    lx->line = 1;
    lx->column = 1;
    lx->peeked = LEX_EMPTY;
    lx->peeked2 = LEX_EMPTY;

    lx->indent_stack[0] = 0;
    lx->indent_top = 0;
    lx->pending_dedents = 0;
    lx->pending_indent = false;
    lx->at_line_start = true;
    lx->line_has_tokens = false;
    lx->paren_depth = 0;

    lx->modes[0] = LEX_MODE_CODE;
    lx->mode_top = 0;

    return strbuf_init(&lx->buf); // only failable step
}

void lexer_destroy(t_lexer *lx)
{
    strbuf_free(&lx->buf); // whoever opened lx->in is responsible for closing it, not the lexer
}

t_lex_mode lexer_mode(const t_lexer *lx)
{
    return lx->modes[lx->mode_top];
}

t_lex_rc lexer_next(t_lexer *lx, t_token *tok)
{
    token_free(tok); // drop the previous token's string

    for (;;)
    {
        // 1. queued layout tokens come first
        if (lx->pending_dedents > 0)
        {
            lx->pending_dedents--;
            token_set(tok, TOKEN_DEDENT, lx->line, lx->column);
            return LEX_OK;
        }
        if (lx->pending_indent)
        {
            lx->pending_indent = false;
            token_set(tok, TOKEN_INDENT, lx->line, lx->column);
            return LEX_OK;
        }

        // 2. inside s"..." strings
        t_lex_mode mode = lexer_mode(lx);
        if (mode == LEX_MODE_ISTR || mode == LEX_MODE_ISTR_MULTI || mode == LEX_MODE_DOLLAR)
        {
            return lexer_scan_istr_part(lx, tok);
        }

        // 3. start of a logical line: work out indentation
        if (lx->at_line_start && lx->paren_depth == 0 && mode == LEX_MODE_CODE)
        {
            LEX_TRY(lexer_handle_indentation(lx));
            continue; // it may have queued INDENT / DEDENTs
        }

        // 4. skip whitespace and comments
        bool had_eol;
        LEX_TRY(lexer_skip_whitespace_and_comments(lx, &had_eol));
        int c = lexer_peekc(lx);
        bool layout = (lx->paren_depth == 0 && mode == LEX_MODE_CODE);

        // a block comment containing a newline acts as an EOL
        if (had_eol && lx->line_has_tokens && layout)
        {
            if (c != '\n' && c != EOF)
            {
                return lexer_error(lx, "token after a multi-line comment on the same line");
            }
            lx->line_has_tokens = false;
            lx->at_line_start = true;
            token_set(tok, TOKEN_EOL, lx->line, lx->column);
            return LEX_OK;
        }

        // newline
        if (c == '\n')
        {
            int line = lx->line;
            int col = lx->column;
            lexer_getc(lx);
            if (lx->line_has_tokens && layout)
            {
                lx->line_has_tokens = false;
                lx->at_line_start = true;
                token_set(tok, TOKEN_EOL, line, col);
                return LEX_OK;
            }
            continue; // blank line or inside parentheses: just whitespace
        }

        // end of file: EOL, then one DEDENT per open block, then EOF
        if (c == EOF)
        {
            if (mode != LEX_MODE_CODE)
            {
                return lexer_error(lx, "unexpected end of file in ${ ... }");
            }
            if (lx->line_has_tokens)
            {
                lx->line_has_tokens = false;
                token_set(tok, TOKEN_EOL, lx->line, lx->column);
                return LEX_OK;
            }
            if (lx->indent_top > 0)
            {
                lx->indent_top--;
                token_set(tok, TOKEN_DEDENT, lx->line, lx->column);
                return LEX_OK;
            }
            token_set(tok, TOKEN_EOF, lx->line, lx->column);
            return LEX_OK;
        }

        return lexer_scan_code(lx, tok);
    }
}