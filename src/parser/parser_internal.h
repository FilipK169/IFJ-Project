/**
 * @file parser_internal.h
 * @brief Private helpers shared between the parser .c files of IFJ26.
 *
 * Only the parser .c files include this header, everybody else includes parser.h.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "parser.h"

#include "ast.h"
#include "lexer.h"
#include "token.h"

#include <stdbool.h>
#include <stdio.h>

// ========================================== //
//         CONSTANTS AND TYPEDEFS         //
// ========================================== //

// Recursive descent parser is limited to this many nested expressions / statements.
#define PARSER_MAX_DEPTH 1000

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

/** Parser state: the lexer, the current token and an optional second one. */
typedef struct {
    t_lexer lexer;
    t_token cur;       // current token
    t_token next;      // one extra look-ahead token, valid only if has_next
    bool has_next;
    bool after_block;  // the last consumed token was the DEDENT closing a block
    int depth;         // current nesting of expressions / statements
} t_parser;

// ========================================== //
//                 MACROS                 //
// ========================================== //

/*
 * AI use
 * Prompt: generate a macro TRY(expr) that evaluates expr, and if it is not PARSE_OK, returns it from the current function. The macro should be usable in any function that returns t_parse_rc. It should evaluate expr only once.
 * @note goto usage was recommended
 */

/** Evaluates a t_parse_rc and returns it from the function if it is not PARSE_OK. */
#define TRY(expr)                                                              \
    do {                                                                       \
        t_parse_rc rc_ = (expr);                                               \
        if (rc_ != PARSE_OK) {                                                 \
            return rc_;                                                        \
        }                                                                      \
    } while (0)

/** Like TRY, but stores the code in the local `rc` and jumps to `fail:` (cleanup). */
#define CHECK(expr)                                                            \
    do {                                                                       \
        rc = (expr);                                                           \
        if (rc != PARSE_OK) {                                                  \
            goto fail;                                                         \
        }                                                                      \
    } while (0)

/*
 * END AI
 */

// ========================================== //
//             INLINE HELPERS             //
// ========================================== //

/** @brief Maps an AST result to a parser code (any AST failure is an internal error). */
static inline t_parse_rc ast_rc(t_ast_result rc) {
    return rc == AST_OK ? PARSE_OK : PARSE_ERR_INTERNAL;
}

/** @brief Source location (line, column) where a token starts. */
static inline SOURCE_LOCATION loc_of(const t_token *tok) {
    SOURCE_LOCATION loc = {(size_t)tok->line, (size_t)tok->column};
    return loc;
}

// ========================================== //
//    DECLARATION OF PRIVATE FUNCTIONS    //
// ========================================== //

// ---------- parser.c: helpers ---------- //

/** @brief Prints "Syntax error at line:col: message" (position of the current token). @return Always PARSE_ERR_SYNTAX. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
t_parse_rc parser_syntax_error(t_parser *p, const char *fmt, ...);

/** @brief malloc'd copy of a string (strdup is POSIX, not C11). NULL on failure. */
char *parser_strdup(const char *s);

/** @brief Moves to the next token; the previous token (and its string) is released. */
t_parse_rc parser_advance(t_parser *p);

/** @brief Type of the token AFTER the current one, nothing is consumed. */
t_parse_rc parser_peek_type(t_parser *p, t_token_type *out);

/** @brief The current token must be `type` (else syntax error); consumes it. */
t_parse_rc parser_expect(t_parser *p, t_token_type type, const char *what);

/** @brief Enters one more nesting level; syntax error when too deep. Pair with parser_leave(). */
t_parse_rc parser_enter(t_parser *p);

/** @brief Leaves a nesting level entered by parser_enter(). */
void parser_leave(t_parser *p);

/** @brief Ends a statement: ';', EOL, or nothing when the statement ended with a block / the block ends. */
t_parse_rc parser_end_statement(t_parser *p);

/** @brief Consumes a ',' inside parentheses; *list_ends is set for a legal trailing comma. */
t_parse_rc parser_skip_comma(t_parser *p, bool *list_ends);

/** @brief Consumes an EOL that is directly followed by `else`. */
t_parse_rc parser_skip_eol_before_else(t_parser *p);

/** @brief Parses a type keyword (Int, Double, String, Unit). */
t_parse_rc parse_type(t_parser *p, ENUM_DATA_TYPE *out);

#endif // PARSER_INTERNAL_H