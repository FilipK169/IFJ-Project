/**
 * @file lexer_string.c
 * @brief String literal scanning of the IFJ26 lexer: "...", """...""" and
 *        the parts of interpolated s"..." / s"""...""" strings.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Ondřej Vondra (x274921)
 */

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "lexer.h"
#include "lexer_internal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/** @brief Decodes the escape after an already consumed '\'. Accepts \\, \", \n, \r and \t. */
static t_lex_rc decode_escape(t_lexer *lx, int *out);

/**
 * @brief Scans the text part of a normal s"..." string (mode LEX_MODE_ISTR).
 *
 * Stops at the closing '"' (pops the mode) or at a single '$' (pushes LEX_MODE_DOLLAR).
 * The text read so far is returned as TOKEN_ISTR_TEXT, possibly empty.
 */
static t_lex_rc scan_istr_text(t_lexer *lx, t_token *tok);

/**
 * @brief Scans what follows a single '$' inside s"..." (mode LEX_MODE_DOLLAR).
 *
 * An identifier pops the mode and returns TOKEN_ISTR_ID, '{' rewrites the mode
 * to LEX_MODE_EXPR and returns TOKEN_ISTR_EXPR_BEGIN.
 */
static t_lex_rc scan_istr_dollar(t_lexer *lx, t_token *tok);

/**
 * @brief Scans the text part of a multiline s"""...""" string (mode LEX_MODE_ISTR_MULTI).
 *
 * Like scan_istr_text, but no escape sequences and newlines are kept.
 */
static t_lex_rc scan_istr_multi(t_lexer *lx, t_token *tok);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static t_lex_rc decode_escape(t_lexer *lx, int *out)
{
    int e = lexer_getc(lx);
    switch (e)
    {
    case '\\':
        *out = '\\';
        return LEX_OK;
    case '"':
        *out = '"';
        return LEX_OK;
    case 'n':
        *out = '\n';
        return LEX_OK;
    case 'r':
        *out = '\r';
        return LEX_OK;
    case 't':
        *out = '\t';
        return LEX_OK;
    case EOF:
    case '\n':
        return lexer_error(lx, "unterminated string");
    default:
        return lexer_error(lx, "invalid escape sequence '\\%c'", e);
    }
}

static t_lex_rc scan_istr_text(t_lexer *lx, t_token *tok)
{
    int line = lx->line;
    int col = lx->column;

    strbuf_clear(&lx->buf);
    for (;;)
    {
        int c = lexer_getc(lx);

        if (c == '"')
        {
            LEX_TRY(lexer_pop_mode(lx)); // the string is finished
            break;
        }
        if (c == '$')
        {
            if (lexer_peekc(lx) != '$')
            {
                LEX_TRY(lexer_push_mode(lx, LEX_MODE_DOLLAR));
                break; // the text read so far is returned
            }
            lexer_getc(lx); // "$$" is one literal '$'
        }
        else if (c == '\\')
        {
            LEX_TRY(decode_escape(lx, &c));
        }
        else if (c == EOF || c == '\n')
        {
            return lexer_error(lx, "unterminated string");
        }
        else if (c < 32 || c > 126)
        {
            return lexer_error(lx, "invalid character (code %d) in string", c);
        }

        LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
    }

    char *text = strbuf_take(&lx->buf);
    if (text == NULL)
    {
        return LEX_ERR_INTERNAL;
    }
    lexer_emit(lx, tok, TOKEN_ISTR_TEXT, line, col);
    tok->value.str_val = text; // set AFTER emit
    return LEX_OK;
}

static t_lex_rc scan_istr_dollar(t_lexer *lx, t_token *tok)
{
    int line = lx->line;
    int col = lx->column;
    int c = lexer_peekc(lx);

    if (c == '{')
    {
        lexer_getc(lx);
        lx->modes[lx->mode_top] = LEX_MODE_EXPR; // rewrite 99 -> -1
        return lexer_emit(lx, tok, TOKEN_ISTR_EXPR_BEGIN, line, col);
    }

    if (!isalpha(c) && c != '_')
    {
        return lexer_error(lx, "expected an identifier or '{' after '$'");
    }

    strbuf_clear(&lx->buf);
    while (isalnum(c) || c == '_')
    {
        lexer_getc(lx);
        LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
        c = lexer_peekc(lx);
    }
    if (lx->buf.len == 1 && lx->buf.data[0] == '_')
    {
        return lexer_error(lx, "'_' alone is not an identifier");
    }

    LEX_TRY(lexer_pop_mode(lx)); // 99 is done, back to the string mode
    char *name = strbuf_take(&lx->buf);
    if (name == NULL)
    {
        return LEX_ERR_INTERNAL;
    }
    lexer_emit(lx, tok, TOKEN_ISTR_ID, line, col);
    tok->value.str_val = name; // set AFTER emit
    return LEX_OK;
}

static t_lex_rc scan_istr_multi(t_lexer *lx, t_token *tok)
{
    int line = lx->line;
    int col = lx->column;

    strbuf_clear(&lx->buf);
    for (;;)
    {
        int c = lexer_getc(lx);

        if (c == EOF)
        {
            return lexer_error(lx, "unterminated multiline string");
        }
        if (c == '"' && lexer_peekc(lx) == '"' && lexer_peekc2(lx) == '"')
        {
            lexer_getc(lx);              // second quote
            lexer_getc(lx);              // third quote
            LEX_TRY(lexer_pop_mode(lx)); // the string is finished
            break;
        }
        if (c == '$')
        {
            if (lexer_peekc(lx) != '$')
            {
                LEX_TRY(lexer_push_mode(lx, LEX_MODE_DOLLAR));
                break; // the text read so far is returned
            }
            lexer_getc(lx); // "$$" is one literal '$'
        }
        else if (c > 127)
        {
            return lexer_error(lx, "non-ASCII character in string");
        }

        LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
    }

    char *text = strbuf_take(&lx->buf);
    if (text == NULL)
    {
        return LEX_ERR_INTERNAL;
    }
    lexer_emit(lx, tok, TOKEN_ISTR_TEXT, line, col);
    tok->value.str_val = text; // set AFTER emit
    return LEX_OK;
}

// ========================================== //
// ==== DEFINITION OF LEXER-WIDE FUNCTIONS === //
// ========================================== //

t_lex_rc lexer_scan_string(t_lexer *lx, t_token *tok)
{
    // the opening '"' is already consumed
    int line = lx->line;
    int col = lx->column - 1;

    strbuf_clear(&lx->buf);
    for (;;)
    {
        int c = lexer_getc(lx);

        if (c == '"')
        {
            break; // closing quote
        }
        if (c == EOF || c == '\n')
        {
            return lexer_error(lx, "unterminated string (started at %d:%d)", line, col);
        }

        if (c == '\\')
        {
            LEX_TRY(decode_escape(lx, &c));
        }
        else if (c < 32 || c > 126)
        {
            return lexer_error(lx, "invalid character (code %d) in string", c);
        }

        LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
    }

    char *text = strbuf_take(&lx->buf);
    if (text == NULL)
    {
        return LEX_ERR_INTERNAL;
    }
    lexer_emit(lx, tok, TOKEN_STRING_LIT, line, col);
    tok->value.str_val = text; // set AFTER emit
    return LEX_OK;
}

t_lex_rc lexer_scan_multiline_string(t_lexer *lx, t_token *tok)
{
    // the opening '"""' is already consumed (all three are on one line)
    int line = lx->line;
    int col = lx->column - 3;

    strbuf_clear(&lx->buf);
    for (;;)
    {
        int c = lexer_getc(lx);

        if (c == EOF)
        {
            return lexer_error(lx, "unterminated multiline string (started at %d:%d)", line, col);
        }
        if (c == '"' && lexer_peekc(lx) == '"' && lexer_peekc2(lx) == '"')
        {
            lexer_getc(lx); // second quote
            lexer_getc(lx); // third quote
            break;
        }
        if (c > 127)
        {
            return lexer_error(lx, "non-ASCII character in string");
        }

        LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
    }

    char *text = strbuf_take(&lx->buf);
    if (text == NULL)
    {
        return LEX_ERR_INTERNAL;
    }
    lexer_emit(lx, tok, TOKEN_STRING_LIT, line, col);
    tok->value.str_val = text; // set AFTER emit
    return LEX_OK;
}

t_lex_rc lexer_scan_istr_part(t_lexer *lx, t_token *tok)
{
    switch (lexer_mode(lx))
    {
    case LEX_MODE_ISTR:
        return scan_istr_text(lx, tok);
    case LEX_MODE_DOLLAR:
        return scan_istr_dollar(lx, tok);
    case LEX_MODE_ISTR_MULTI:
        return scan_istr_multi(lx, tok);
    default:
        return LEX_ERR_INTERNAL; // should never happen, but we don't want to crash
    }
}