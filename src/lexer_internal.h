/**
 * @file lexer_internal.h
 * @brief Private helpers shared between the lexer .c files of IFJ26.
 *
 * Only the lexer .c files include this header, the parser and main only
 * include lexer.h. Anything used by a single .c file is static in that file
 * and is NOT declared here.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Ondřej Vondra (x274921)
 */

#ifndef LEXER_INTERNAL_H
#define LEXER_INTERNAL_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "lexer.h"
#include "token.h"

#include <stdbool.h>

// ========================================== //
// ================ MACROS ================== //
// ========================================== //

#define LEX_EMPTY (-2) // marker of an empty look-ahead slot (EOF is -1)

/*
 * AI use
 * Prompt: macro that evaluates an expression and returns it from the current function if it is not
 * LEX_OK
 * @note the macro was then later adjused to the project environment
 */

/**
 * @brief Evaluates `expr` (a t_lex_rc) and returns it from the current function if it is not
 * LEX_OK.
 */
#define LEX_TRY(expr)                                                                              \
    do                                                                                             \
    {                                                                                              \
        t_lex_rc rc_ = (expr);                                                                     \
        if (rc_ != LEX_OK)                                                                         \
        {                                                                                          \
            return rc_;                                                                            \
        }                                                                                          \
    } while (0)

/*
 * END AI
 */

// ========================================== //
// ============ INLINE HELPERS ============== //
// ========================================== //

/**
 * AI use
 * Prompt: inline function that fills a token with type, line and column, zeroes the value, and
 * marks the current logical line as non-empty
 * @note the function was then later adjused to the project environment
 * @note AI was mainly used to discover what inline is
 */

/**
 * @brief Fills *tok with a token with no value and marks the current logical line as non-empty.
 *
 * Note: token_set zeroes the token, so a value (e.g. str_val) must be assigned AFTER calling this
 * function. static inline = every .c file gets its own private copy, nothing is exported.
 *
 * @return Always LEX_OK (so scanners can write `return lexer_emit(...);`).
 */
static inline t_lex_rc lexer_emit(t_lexer *lx, t_token *tok, t_token_type type, int line, int col)
{
    token_set(tok, type, line, col);
    lx->line_has_tokens = true;
    return LEX_OK;
}

// ========================================== //
// ===== DECLARATION OF PRIVATE FUNCTIONS === //
// ========================================== //

// ---------- lexer_chars.c: string buffer ---------- //

/** @brief Initializes an empty string buffer. @retval LEX_ERR_INTERNAL malloc failed */
t_lex_rc strbuf_init(t_strbuf *sb);

/** @brief Clears the buffer (len = 0), keeps the allocated memory. */
void strbuf_clear(t_strbuf *sb);

/** @brief Appends one character (grows the buffer when needed). @retval LEX_ERR_INTERNAL realloc
 * failed */
t_lex_rc strbuf_append_char(t_strbuf *sb, char c);

/** @brief Returns a malloc'd copy of the content (caller frees), then clears the buffer. NULL on
 * failure. */
char *strbuf_take(t_strbuf *sb);

/** @brief Frees the memory owned by the buffer. */
void strbuf_free(t_strbuf *sb);

// ---------- lexer_chars.c: error reporting ---------- //

/*
 * AI use
 * Prompt: error function with a variable amount of arguments that prints the error location and
 * message according to the input format string
 */

/**
 * @brief Prints "Lexical error at line:col: message" to stderr.
 *
 * Does not exit and does not free anything. Use as `return lexer_error(lx, "...");`.
 *
 * @return Always LEX_ERR_LEXICAL.
 */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
t_lex_rc lexer_error(t_lexer *lx, const char *fmt, ...);

/*
 * END AI
 */

// ---------- lexer_chars.c: character level ---------- //

/** @brief Reads a char and updates line/column. Returns the char, or EOF. */
int lexer_getc(t_lexer *lx);

/** @brief Looks at the next char without consuming it. */
int lexer_peekc(t_lexer *lx);

/** @brief Looks at the char after the next one without consuming anything. */
int lexer_peekc2(t_lexer *lx);

/** @brief Skips characters up to the end of the line (the '\n' itself is NOT consumed). */
void lexer_skip_to_eol(t_lexer *lx);

// ---------- lexer_chars.c: mode stack ---------- //

/** @brief Pushes a mode. @retval LEX_ERR_LEXICAL mode stack is full */
t_lex_rc lexer_push_mode(t_lexer *lx, t_lex_mode mode);

/** @brief Pops the current mode. @retval LEX_ERR_INTERNAL only the base mode is left */
t_lex_rc lexer_pop_mode(t_lexer *lx);

// ---------- lexer_layout.c ---------- //

/**
 * @brief Skips spaces, tabs, '\r', line comments and (nested) block comments.
 *
 * Stops in front of '\n', EOF or the first character of a real token.
 * The '\n' itself is never consumed.
 *
 * @param[out] had_eol Set to true if a block comment containing a newline was skipped.
 *
 * @retval LEX_ERR_LEXICAL unterminated block comment
 */
t_lex_rc lexer_skip_whitespace_and_comments(t_lexer *lx, bool *had_eol);

/**
 * @brief Handles indentation at the start of a logical line.
 *
 * Only queues INDENT / DEDENT (pending_indent / pending_dedents),
 * the tokens themselves are emitted by lexer_next.
 *
 * @retval LEX_ERR_LEXICAL tab indentation or inconsistent dedent
 */
t_lex_rc lexer_handle_indentation(t_lexer *lx);

// ---------- lexer_scan.c ---------- //

/**
 * @brief Scans a token in code mode (the big START state).
 *
 * Layout (newlines, indentation) is handled by lexer_next before this is called.
 * '}' is only valid in LEX_MODE_EXPR, where it ends a ${ ... }.
 */
t_lex_rc lexer_scan_code(t_lexer *lx, t_token *tok);

// ---------- lexer_string.c ---------- //

/** @brief Scans a "..." string literal (the opening quote is already consumed). */
t_lex_rc lexer_scan_string(t_lexer *lx, t_token *tok);

/** @brief Scans a """...""" string literal (the opening quotes are already consumed). */
t_lex_rc lexer_scan_multiline_string(t_lexer *lx, t_token *tok);

/**
 * @brief Scans one part inside an interpolated string, chosen by the mode:
 *        text (modes 1 and 3), or what follows a '$' (mode 99).
 *
 * Text parts end at the closing quote(s) or at a single '$' and are returned
 * as TOKEN_ISTR_TEXT (possibly empty). After a '$' the result is
 * TOKEN_ISTR_ID or TOKEN_ISTR_EXPR_BEGIN.
 */
t_lex_rc lexer_scan_istr_part(t_lexer *lx, t_token *tok);

#endif // LEXER_INTERNAL_H