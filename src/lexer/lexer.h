/**
 * @file lexer.h
 * @brief Lexical analyzer public interface for IFJ26.
 *
 * This is the only lexer header the parser and main should include.
 * Private helpers live in lexer_internal.h.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef LEXER_H
#define LEXER_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "token.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// ========================================== //
// ======== CONSTANTS AND TYPEDEFS ========== //
// ========================================== //

#define LEXER_MAX_INDENT 256 // max nesting of indented blocks
#define LEXER_MAX_MODES 64   // max nesting of string / ${...} modes
                             // (each s"${ level uses two slots)

/**
 * @brief Return codes of the lexer functions.
 *
 * The numeric values are the compiler exit codes.
 */
typedef enum {
    LEX_OK = 0,           // success (*tok holds a valid token, possibly TOKEN_EOF)
    LEX_ERR_LEXICAL = 1,  // lexical error, message already printed
    LEX_ERR_INTERNAL = 99 // malloc failed, internal bug
} t_lex_rc;

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

/**
 * @brief Lexer modes.
 *
 * Inside s"..." strings the lexer follows different rules than normal code,
 * so it keeps a stack of modes. The top of the stack decides how the next
 * token is scanned. The parser reads it through lexer_mode() to know where it
 * is inside an interpolated string, so the numeric values are part of the
 * interface:
 *   0   normal code
 *   1   inside s"..."   (pushed by s", popped by the closing ")
 *   3   inside s"""..."""   (pushed by s""", popped by the closing """)
 *   99  right after a single '$' (popped after the id, rewritten to -1 by '{')
 *   -1  inside ${ ... } (code-like, popped by '}')
 */
typedef enum {
    LEX_MODE_EXPR = -1,      // inside  ${ ... }  (code-like, '}' ends it)
    LEX_MODE_CODE = 0,       // normal code
    LEX_MODE_ISTR = 1,       // inside  s"..."
    LEX_MODE_ISTR_MULTI = 3, // inside  s"""..."""
    LEX_MODE_DOLLAR = 99     // after a single '$': an id or '{' must follow
} t_lex_mode;

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

/**
 * @brief Small growable string buffer.
 *
 * Only the type is public (t_lexer holds one by value). Do not touch the
 * fields from outside the lexer; the strbuf_* functions are in lexer_internal.h.
 */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} t_strbuf;

/**
 * @brief Lexer state.
 *
 * Treat as opaque: only the lexer .c files read or write the fields.
 */
typedef struct {
    FILE *in;

    /* position tracking */
    int line;
    int column;
    int peeked;  // one char of look-ahead (EOF-capable), -2 = empty
    int peeked2; // second look-ahead char, -2 = empty (needed for """ and "..")

    /* layout / indentation */
    int indent_stack[LEXER_MAX_INDENT];
    int indent_top;       // index of the top of indent_stack (stack[0] == 0)
    int pending_dedents;  // DEDENT tokens still to be emitted
    bool pending_indent;  // an INDENT token is still to be emitted
    bool at_line_start;   // next char begins a new physical line
    bool line_has_tokens; // current logical line already produced a token
    int paren_depth;      // depth of '(' ... ')' -> newlines are ignored inside

    /* string interpolation */
    t_lex_mode modes[LEXER_MAX_MODES];
    int mode_top; // index of current mode

    /* scratch buffer reused while scanning lexemes */
    t_strbuf buf;
} t_lexer;

// ========================================== //
// ====== DECLARATION OF PUBLIC FUNCTIONS === //
// ========================================== //

/**
 * @brief Initializes the lexer to read from `in`.
 *
 * @param[out] lx Lexer to initialize.
 * @param[in] in Input stream to read from.
 *
 * @return Result code.
 * @retval LEX_OK - lexer initialized
 * @retval LEX_ERR_INTERNAL - memory allocation failed
 */
t_lex_rc lexer_init(t_lexer *lx, FILE *in);

/**
 * @brief Releases everything the lexer owns (does NOT close `in`).
 *
 * The current token is owned by the parser, which frees it with token_free().
 *
 * @param[in,out] lx Lexer to destroy.
 */
void lexer_destroy(t_lexer *lx);

/**
 * @brief Scans the next token into `*tok`.
 *
 * Frees the previous content of `*tok` first (token_free), so `*tok` must have
 * been initialized with token_init() before the first call. Inside an
 * interpolated string one call returns one part (ISTR_TEXT, ISTR_ID,
 * ISTR_EXPR_BEGIN, ISTR_EXPR_END). End of input is
 * LEX_OK with tok->type == TOKEN_EOF (and keeps being returned on further
 * calls). On error `*tok` holds no string, so it is still safe to free.
 *
 * @param[in,out] lx Lexer to read from.
 * @param[in,out] tok Token to overwrite with the next token.
 *
 * @return Result code.
 * @retval LEX_OK - *tok holds a valid token
 * @retval LEX_ERR_LEXICAL - lexical error (message already printed)
 * @retval LEX_ERR_INTERNAL - memory allocation failed / internal bug
 */
t_lex_rc lexer_next(t_lexer *lx, t_token *tok);

/**
 * @brief Returns the current (top) lexer mode.
 *
 * The parser uses this to know whether it is inside an interpolated string.
 * Compare against the t_lex_mode names, never the raw numbers.
 *
 * @param[in] lx Lexer to query.
 *
 * @return The current mode.
 */
t_lex_mode lexer_mode(const t_lexer *lx);

#endif // LEXER_H