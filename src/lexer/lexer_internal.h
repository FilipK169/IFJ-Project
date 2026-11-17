/**
 * @file lexer_internal.h
 * @brief Private helpers of the IFJ26 lexical analyzer.
 *
 * Only the lexer's own .c files (lexer.c, lexer_scan.c, ...) include this
 * header. The parser and main must use lexer.h only.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
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

/**
 * @brief Evaluates `expr` (a t_lex_rc) and returns it from the current function if it is not LEX_OK.
 * Used to skip writing rc != LEX_OK checks after every call.
 */
#define LEX_TRY(expr)                                                          \
    do {                                                                       \
        t_lex_rc rc_ = (expr);                                                 \
        if (rc_ != LEX_OK) {                                                   \
            return rc_;                                                        \
        }                                                                      \
    } while (0)

// ========================================== //
// ===== DECLARATION OF PRIVATE FUNCTIONS === //
// ========================================== //

// ---------- string buffer ---------- //

/**
 * @brief Initializes an empty string buffer.
 *
 * @param[out] sb Buffer to initialize.
 *
 * @return Result code.
 * @retval LEX_OK - buffer initialized
 * @retval LEX_ERR_INTERNAL - memory allocation failed
 */
t_lex_rc strbuf_init(t_strbuf *sb);

/**
 * @brief Clears the buffer (len = 0), keeps the allocated memory.
 *
 * @param[in,out] sb Buffer to clear.
 */
void strbuf_clear(t_strbuf *sb);

/**
 * @brief Appends one character to the buffer (grows it when needed).
 *
 * @param[in,out] sb Buffer to append to.
 * @param[in] c Character to append.
 *
 * @return Result code.
 * @retval LEX_OK - character appended
 * @retval LEX_ERR_INTERNAL - memory allocation failed
 */
t_lex_rc strbuf_append_char(t_strbuf *sb, char c);

/**
 * @brief Returns a malloc'd copy of the buffer content, then clears the buffer.
 *
 * @param[in,out] sb Buffer to take the content from.
 *
 * @return Heap allocated NUL-terminated string, the caller must free it.
 * @retval NULL - memory allocation failed
 */
char *strbuf_take(t_strbuf *sb);

/**
 * @brief Frees the memory owned by the buffer.
 *
 * @param[in,out] sb Buffer to free.
 */
void strbuf_free(t_strbuf *sb);

// ---------- error reporting ---------- //

/**
 * @brief Prints "Lexical error at line:col: message" to stderr.
 *
 * Does not exit and does not free anything. Use it as
 * `return lexer_error(lx, "...");`.
 *
 * @param[in,out] lx Lexer in which the error occurred.
 * @param[in] fmt printf-style format string of the message.
 *
 * @return Always LEX_ERR_LEXICAL.
 */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
t_lex_rc lexer_error(t_lexer *lx, const char *fmt, ...);

// ---------- character level ---------- //

/**
 * @brief Reads a char and updates line/column.
 *
 * @param[in,out] lx Lexer to read from.
 *
 * @return The read character, or EOF.
 */
int lexer_getc(t_lexer *lx);

/**
 * @brief Looks at the next char without consuming it.
 *
 * @param[in,out] lx Lexer to read from.
 *
 * @return The next character, or EOF.
 */
int lexer_peekc(t_lexer *lx);

/**
 * @brief Looks at the char after the next one without consuming anything.
 *
 * @param[in,out] lx Lexer to read from.
 *
 * @return The second next character, or EOF.
 */
int lexer_peekc2(t_lexer *lx);

/**
 * @brief Skips characters up to the end of the current line (the '\n' itself
 *        is NOT consumed).
 *
 * @param[in,out] lx Lexer to read from.
 */
void lexer_skip_to_eol(t_lexer *lx);

// ---------- mode stack ---------- //

/**
 * @brief Pushes a new mode on the mode stack.
 *
 * @param[in,out] lx Lexer to modify.
 * @param[in] mode Mode to push.
 *
 * @return Result code.
 * @retval LEX_OK - mode pushed
 * @retval LEX_ERR_LEXICAL - mode stack is full (nesting too deep)
 */
t_lex_rc lexer_push_mode(t_lexer *lx, t_lex_mode mode);

/**
 * @brief Pops the current mode from the mode stack.
 *
 * @param[in,out] lx Lexer to modify.
 *
 * @return Result code.
 * @retval LEX_OK - mode popped
 * @retval LEX_ERR_INTERNAL - the stack only holds the base mode
 */
t_lex_rc lexer_pop_mode(t_lexer *lx);

// ---------- layout ---------- //

/**
 * @brief Handles indentation, run at the start of a logical line.
 *
 * Only queues INDENT / DEDENT (pending_indent / pending_dedents), tokens are
 * emitted by lexer_next.
 *
 * @param[in,out] lx Lexer to modify.
 *
 * @return Result code.
 * @retval LEX_OK - indentation handled
 * @retval LEX_ERR_LEXICAL - tab indentation or inconsistent dedent
 */
t_lex_rc lexer_handle_indentation(t_lexer *lx);

// ---------- scanners ---------- //

/**
 * @brief Scans a token in code mode (the big START state).
 *
 * Layout (newlines, indentation) is handled by lexer_next before this is
 * called. '}' is only valid in LEX_MODE_EXPR, where it ends a ${ ... }.
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] tok Token to fill.
 *
 * @return Result code.
 * @retval LEX_OK - token scanned
 * @retval LEX_ERR_LEXICAL - unknown character or malformed lexeme
 * @retval LEX_ERR_INTERNAL - memory allocation failed
 */
t_lex_rc lexer_scan_code(t_lexer *lx, t_token *tok);

/**
 * @brief Scans an identifier or keyword.
 *
 * An identifier that is exactly "s" followed right away by '"' starts an
 * interpolated string instead: it pushes the string mode and returns the first
 * ISTR_TEXT part. A lone '_' is not an identifier (lexical error).
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] tok Token to fill.
 * @param[in] first First (already consumed) character of the lexeme.
 *
 * @return Result code.
 */
t_lex_rc lexer_scan_identifier(t_lexer *lx, t_token *tok, int first);

/**
 * @brief Scans an integer or double literal.
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] tok Token to fill.
 * @param[in] first First (already consumed) character of the lexeme: a digit,
 *                  or '.' when the caller has seen a digit right after it.
 *
 * @return Result code.
 */
t_lex_rc lexer_scan_number(t_lexer *lx, t_token *tok, int first);

/**
 * @brief Scans a "..." string literal (the opening quote is consumed).
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] tok Token to fill.
 *
 * @return Result code.
 */
t_lex_rc lexer_scan_string(t_lexer *lx, t_token *tok);

/**
 * @brief Scans a """...""" multiline string literal (the opening quotes are
 *        consumed).
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] tok Token to fill.
 *
 * @return Result code.
 */
t_lex_rc lexer_scan_multiline_string(t_lexer *lx, t_token *tok);

/**
 * @brief Scans one part inside an interpolated string, chosen by the mode:
 *        text (modes 1 and 3), or what follows a '$' (mode 99).
 *
 * Text parts end at the closing quote(s) or at a single '$' and are returned
 * as TOKEN_ISTR_TEXT (possibly empty). After a '$' the result is TOKEN_ISTR_ID
 * or TOKEN_ISTR_EXPR_BEGIN.
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] tok Token to fill.
 *
 * @return Result code.
 */
t_lex_rc lexer_scan_istr_part(t_lexer *lx, t_token *tok);

/**
 * @brief Skips spaces, tabs, '\r', line comments and (nested) block comments.
 *
 * Stops in front of '\n', EOF or the first character of a real token. The
 * '\n' itself is never consumed.
 *
 * @param[in,out] lx Lexer to read from.
 * @param[out] had_eol Set to true if a block comment containing a newline was
 *                     skipped (the caller then treats it as an EOL and checks
 *                     that nothing but whitespace follows on that line).
 *
 * @return Result code.
 * @retval LEX_OK - skipped successfully
 * @retval LEX_ERR_LEXICAL - unterminated block comment
 */
t_lex_rc lexer_skip_whitespace_and_comments(t_lexer *lx, bool *had_eol);

// ---------- keyword table ---------- //

/**
 * @brief Looks up a keyword by its name.
 *
 * @param[in] name NUL-terminated lexeme to look up.
 *
 * @return Matching keyword token type, or TOKEN_ID if it is not a keyword.
 */
t_token_type lexer_keyword_lookup(const char *name);

#endif // LEXER_INTERNAL_H