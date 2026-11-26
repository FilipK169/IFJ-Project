/**
 * @file lexer_scan.c
 * @brief Scanning of code-mode tokens in the IFJ26 lexer: operators,
 *        identifiers, keywords and numbers.
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

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
// ========== PRIVATE STRUCTURES ============ //
// ========================================== //

// 1 row of the keyword table.
typedef struct
{
    const char *name;
    t_token_type type;
} t_keyword;

static const t_keyword KEYWORDS[] = {
    {"def", TOKEN_KW_DEF},
    {"do", TOKEN_KW_DO},
    {"Double", TOKEN_KW_DOUBLE},
    {"else", TOKEN_KW_ELSE},
    {"if", TOKEN_KW_IF},
    {"import", TOKEN_KW_IMPORT},
    {"Int", TOKEN_KW_INT},
    {"return", TOKEN_KW_RETURN},
    {"String", TOKEN_KW_STRING},
    {"then", TOKEN_KW_THEN},
    {"Unit", TOKEN_KW_UNIT},
    {"val", TOKEN_KW_VAL},
    {"var", TOKEN_KW_VAR},
    {"while", TOKEN_KW_WHILE},
    // extensions
    {"true", TOKEN_KW_TRUE},
    {"false", TOKEN_KW_FALSE},
    {"Boolean", TOKEN_KW_BOOLEAN},
    {"for", TOKEN_KW_FOR},
};

#define KEYWORD_COUNT (sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/** @brief Consumes the next char only if it equals `expected`. Returns whether it matched. */
static bool match(t_lexer *lx, int expected);

/**
 * @brief Appends the following digits to lx->buf; underscores between digits are skipped.
 *
 * An underscore is only valid when a digit comes right after it ("1_000" ok, "1_", "1__0" errors).
 */
static t_lex_rc read_digits(t_lexer *lx);

/** @brief Returns the keyword token type of `name`, or TOKEN_ID if it is not a keyword. */
static t_token_type lexer_keyword_lookup(const char *name);

/**
 * @brief Scans an identifier or keyword (`first` is already consumed).
 *
 * An identifier that is exactly "s" followed right away by '"' starts an interpolated
 * string instead: it pushes the string mode and returns the first ISTR_TEXT part.
 */
static t_lex_rc lexer_scan_identifier(t_lexer *lx, t_token *tok, int first);

/** @brief Scans an integer or double literal (`first` is a digit, or '.' followed by a digit). */
static t_lex_rc lexer_scan_number(t_lexer *lx, t_token *tok, int first);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static bool match(t_lexer *lx, int expected)
{
    if (lexer_peekc(lx) == expected)
    {
        lexer_getc(lx);
        return true;
    }
    return false;
}

static t_lex_rc read_digits(t_lexer *lx)
{
    for (;;)
    {
        int c = lexer_peekc(lx);
        if (isdigit(c))
        {
            lexer_getc(lx);
            LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
        }
        else if (c == '_')
        {
            if (!isdigit(lexer_peekc2(lx)))
            {
                return lexer_error(lx, "misplaced '_' in number");
            }
            lexer_getc(lx); // skip the '_', the digit is read next round
        }
        else
        {
            return LEX_OK;
        }
    }
}

static t_token_type lexer_keyword_lookup(const char *name)
{
    for (size_t i = 0; i < KEYWORD_COUNT; i++)
    {
        if (strcmp(name, KEYWORDS[i].name) == 0)
        {
            return KEYWORDS[i].type;
        }
    }
    return TOKEN_ID;
}

static t_lex_rc lexer_scan_identifier(t_lexer *lx, t_token *tok, int first)
{
    // `first` is already consumed, so the token started one column back
    int line = lx->line;
    int col = lx->column - 1;

    strbuf_clear(&lx->buf);
    LEX_TRY(strbuf_append_char(&lx->buf, (char)first));

    int c = lexer_peekc(lx);
    while (isalnum(c) || c == '_')
    {
        lexer_getc(lx);
        LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
        c = lexer_peekc(lx);
    }

    // s"..." or s"""...""" : the identifier is exactly "s", a '"' follows
    if (lx->buf.len == 1 && lx->buf.data[0] == 's' && lexer_peekc(lx) == '"')
    {
        lexer_getc(lx); // the opening '"'
        if (lexer_peekc(lx) == '"' && lexer_peekc2(lx) == '"')
        {
            lexer_getc(lx); // second quote
            lexer_getc(lx); // third quote
            LEX_TRY(lexer_push_mode(lx, LEX_MODE_ISTR_MULTI));
        }
        else
        {
            LEX_TRY(lexer_push_mode(lx, LEX_MODE_ISTR));
        }
        LEX_TRY(lexer_scan_istr_part(lx, tok));
        tok->line = line; // the token starts at the 's'
        tok->column = col;
        return LEX_OK;
    }

    if (lx->buf.len == 1 && lx->buf.data[0] == '_')
    {
        return lexer_error(lx, "'_' alone is not an identifier");
    }

    t_token_type type = lexer_keyword_lookup(lx->buf.data);
    if (type != TOKEN_ID)
    {
        return lexer_emit(lx, tok, type, line, col); // keyword: no string needed
    }

    char *name = strbuf_take(&lx->buf);
    if (name == NULL)
    {
        return LEX_ERR_INTERNAL;
    }
    lexer_emit(lx, tok, TOKEN_ID, line, col);
    tok->value.str_val = name; // set AFTER emit: token_set zeroes the value
    return LEX_OK;
}

static t_lex_rc lexer_scan_number(t_lexer *lx, t_token *tok, int first)
{
    // `first` is already consumed (a digit or '.', never a newline)
    int line = lx->line;
    int col = lx->column - 1;
    bool is_double = false;

    strbuf_clear(&lx->buf);
    LEX_TRY(strbuf_append_char(&lx->buf, (char)first));

    if (first == '.')
    {
        // caller guarantees a digit follows, so this reads the fraction
        is_double = true;
        LEX_TRY(read_digits(lx));
    }
    else
    {
        // integer part
        if (first != '0')
        {
            LEX_TRY(read_digits(lx));
        }
        else if (isdigit(lexer_peekc(lx)))
        {
            return lexer_error(lx, "leading zeros are not allowed");
        }

        // fraction: '.' only counts if a digit follows
        if (lexer_peekc(lx) == '.' && isdigit(lexer_peekc2(lx)))
        {
            lexer_getc(lx);
            LEX_TRY(strbuf_append_char(&lx->buf, '.'));
            is_double = true;
            LEX_TRY(read_digits(lx));
        }
    }

    // exponent: e/E, optional sign, at least one digit
    int c = lexer_peekc(lx);
    if (c == 'e' || c == 'E')
    {
        lexer_getc(lx);
        LEX_TRY(strbuf_append_char(&lx->buf, 'e'));
        is_double = true;

        c = lexer_peekc(lx);
        if (c == '+' || c == '-')
        {
            lexer_getc(lx);
            LEX_TRY(strbuf_append_char(&lx->buf, (char)c));
        }
        if (!isdigit(lexer_peekc(lx)))
        {
            return lexer_error(lx, "exponent has no digits");
        }
        LEX_TRY(read_digits(lx));
    }

    // optional d/D suffix, only after a fraction or exponent
    if (is_double && (lexer_peekc(lx) == 'd' || lexer_peekc(lx) == 'D'))
    {
        lexer_getc(lx); // not appended, strtod would not understand it
    }

    // a letter, digit or '_' stuck to the number (12abc, 1.5d4)
    int next = lexer_peekc(lx);
    if (isalnum(next) || next == '_')
    {
        return lexer_error(lx, "invalid character '%c' after number", next);
    }

    errno = 0;
    if (is_double)
    {
        double value = strtod(lx->buf.data, NULL);
        if (errno == ERANGE)
        {
            return lexer_error(lx, "double literal is out of range");
        }
        lexer_emit(lx, tok, TOKEN_DOUBLE_LIT, line, col);
        tok->value.double_val = value; // set AFTER emit
    }
    else
    {
        long long value = strtoll(lx->buf.data, NULL, 10);
        if (errno == ERANGE)
        {
            return lexer_error(lx, "integer literal is too large");
        }
        lexer_emit(lx, tok, TOKEN_INT_LIT, line, col);
        tok->value.int_val = value; // set AFTER emit
    }
    return LEX_OK;
}

// ========================================== //
// ===== DEFINITION OF LEXER-WIDE FUNCTION ==== //
// ========================================== //

t_lex_rc lexer_scan_code(t_lexer *lx, t_token *tok)
{
    int line = lx->line; // save position BEFORE consuming the first char
    int col = lx->column;
    int c = lexer_getc(lx);

    switch (c)
    {
    case EOF:
        token_set(tok, TOKEN_EOF, line, col); // EOF does not set line_has_tokens
        return LEX_OK;

    case '+':
        return lexer_emit(lx, tok, TOKEN_PLUS, line, col);
    case '-':
        return lexer_emit(lx, tok, TOKEN_MINUS, line, col);
    case '*':
        return lexer_emit(lx, tok, TOKEN_MULTIPLY, line, col);
    case ',':
        return lexer_emit(lx, tok, TOKEN_COMMA, line, col);
    case ';':
        return lexer_emit(lx, tok, TOKEN_SEMICOLON, line, col);
    case ':':
        return lexer_emit(lx, tok, TOKEN_COLON, line, col);

    case '(':
        lx->paren_depth++;
        return lexer_emit(lx, tok, TOKEN_LEFT_PAREN, line, col);
    case ')':
        if (lx->paren_depth == 0)
        {
            return lexer_error(lx, "unmatched ')'");
        }
        lx->paren_depth--;
        return lexer_emit(lx, tok, TOKEN_RIGHT_PAREN, line, col);

    case '=':
        return lexer_emit(lx, tok, match(lx, '=') ? TOKEN_EQUAL : TOKEN_ASSIGN, line, col);

    case '>':
        return lexer_emit(lx, tok, match(lx, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER, line, col);

    case '!':
        return lexer_emit(lx, tok, match(lx, '=') ? TOKEN_NOT_EQUAL : TOKEN_NOT, line, col);

    case '<':
        if (match(lx, '='))
        {
            return lexer_emit(lx, tok, TOKEN_LESS_EQUAL, line, col);
        }
        if (match(lx, '-'))
        {
            return lexer_emit(lx, tok, TOKEN_LEFT_ARROW, line, col);
        }
        return lexer_emit(lx, tok, TOKEN_LESS, line, col);

    case '&':
        if (match(lx, '&'))
        {
            return lexer_emit(lx, tok, TOKEN_AND, line, col);
        }
        return lexer_error(lx, "unexpected '&'");

    case '|':
        return lexer_emit(lx, tok, match(lx, '|') ? TOKEN_OR : TOKEN_UNION, line, col);

    case '.':
        if (isdigit(lexer_peekc(lx)))
        {
            return lexer_scan_number(lx, tok, '.');
        }
        return lexer_emit(lx, tok, TOKEN_DOT, line, col);

    case '/':
        return lexer_emit(lx, tok, TOKEN_DIVIDE, line, col);

    case '@':
    {
        for (const char *p = "main"; *p != '\0'; p++)
        {
            if (!match(lx, *p))
            {
                return lexer_error(lx, "expected 'main' right after '@'");
            }
        }
        int next = lexer_peekc(lx);
        if (isalnum(next) || next == '_')
        {
            return lexer_error(lx, "invalid character '%c' after '@main'", next);
        }
        return lexer_emit(lx, tok, TOKEN_ANNOT_MAIN, line, col);
    }

    case '"':
        if (lexer_peekc(lx) == '"' && lexer_peekc2(lx) == '"')
        {
            lexer_getc(lx); // second quote
            lexer_getc(lx); // third quote
            return lexer_scan_multiline_string(lx, tok);
        }
        return lexer_scan_string(lx, tok);

    case '}':
        if (lexer_mode(lx) != LEX_MODE_EXPR)
        {
            return lexer_error(lx, "unexpected '}'");
        }
        LEX_TRY(lexer_pop_mode(lx)); // back to the string mode
        return lexer_emit(lx, tok, TOKEN_ISTR_EXPR_END, line, col);

    default:
        if (isdigit(c))
        {
            return lexer_scan_number(lx, tok, c);
        }
        if (isalpha(c) || c == '_')
        {
            return lexer_scan_identifier(lx, tok, c);
        }
        return lexer_error(lx, "unknown character '%c'", c);
    }
}