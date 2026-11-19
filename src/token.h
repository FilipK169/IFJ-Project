/**
 * @file token.h
 * @brief Token definitions for the IFJ26 lexer.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef TOKEN_H
#define TOKEN_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include <stddef.h>

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

/**
 * @brief All token types the lexer can produce.
 */
typedef enum {
    TOKEN_EOF,

    // Identifiers and literals
    TOKEN_ID,
    TOKEN_INT_LIT,
    TOKEN_DOUBLE_LIT,
    TOKEN_STRING_LIT,

    // Keywords
    TOKEN_KW_DEF,
    TOKEN_KW_DO,
    TOKEN_KW_DOUBLE, // type Double
    TOKEN_KW_ELSE,
    TOKEN_KW_IF,
    TOKEN_KW_IMPORT,
    TOKEN_KW_INT, // type Int
    TOKEN_KW_RETURN,
    TOKEN_KW_STRING, // type String
    TOKEN_KW_THEN,
    TOKEN_KW_UNIT, // type Unit
    TOKEN_KW_VAL,
    TOKEN_KW_VAR,
    TOKEN_KW_WHILE,

    // Keywords from extensions
    TOKEN_KW_TRUE,    // BOOLTHEN
    TOKEN_KW_FALSE,   // BOOLTHEN
    TOKEN_KW_BOOLEAN, // BOOLTHEN (type Boolean)
    TOKEN_KW_FOR,     // CONTROL

    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_EQUAL,     // ==
    TOKEN_NOT_EQUAL, // !=

    // Operators from extensions
    TOKEN_NOT, // !   (BOOLTHEN)
    TOKEN_AND, // &&  (BOOLTHEN)
    TOKEN_OR,  // ||  (BOOLTHEN)

    // Syntax
    TOKEN_ASSIGN, // =
    TOKEN_COLON,
    TOKEN_DOT,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_ANNOT_MAIN, // @main
    TOKEN_LEFT_ARROW, // <-  (CONTROL)
    TOKEN_UNION,      // |   (ALGEBDT)

    // Interpolated strings (INTERPOLATEDS)
    TOKEN_ISTR_BEGIN,      // s"  or  s"""
    TOKEN_ISTR_TEXT,       // literal text part (decoded)
    TOKEN_ISTR_ID,         // $name
    TOKEN_ISTR_EXPR_BEGIN, // ${
    TOKEN_ISTR_EXPR_END,   // }
    TOKEN_ISTR_END,        // closing " or """

    // Indentation
    TOKEN_EOL,
    TOKEN_INDENT,
    TOKEN_DEDENT

} t_token_type;

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

/**
 * @brief A single token produced by the lexer.
 *
 * Which member of `value` is valid depends on `type`:
 *   TOKEN_INT_LIT                      -> value.int_val
 *   TOKEN_DOUBLE_LIT                   -> value.double_val
 *   TOKEN_ID, TOKEN_STRING_LIT,
 *   TOKEN_ISTR_TEXT, TOKEN_ISTR_ID     -> value.str_val (heap allocated,
 *                                         NUL-terminated, escapes already decoded)
 *   everything else                    -> value is unused
 *
 * The token is owned by whoever holds it (normally the parser). A token that
 * owns a string must be released with token_free().
 */
typedef struct {
    t_token_type type;
    int line;   // 1-based line where the token starts
    int column; // 1-based column where the token starts
    union {
        long long int_val;
        double double_val;
        char *str_val;
    } value;
} t_token;

/*
 * t_token
 * │
 * ├── t_token_type type
 * ├── int line
 * ├── int column
 * └── union value
 *         ├── long long int_val   (TOKEN_INT_LIT)
 *         ├── double double_val   (TOKEN_DOUBLE_LIT)
 *         └── char *str_val       (TOKEN_ID, TOKEN_STRING_LIT,
 *                                  TOKEN_ISTR_TEXT, TOKEN_ISTR_ID)
 */

// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

/**
 * @brief Human readable name of a token type (for debugging / error messages).
 *
 * @param[in] type Token type to get the name of.
 *
 * @return Static NUL-terminated name of the token type.
 */
const char *token_type_name(t_token_type type);

/**
 * @brief Checks whether the token type owns heap memory in value.str_val.
 *
 * @param[in] type Token type to check.
 *
 * @return Whether the token type owns a heap allocated string.
 * @retval 1 - the type owns heap memory in value.str_val
 * @retval 0 - the type owns no heap memory
 */
int token_has_string(t_token_type type);

/**
 * @brief Frees memory owned by the token (safe to call twice).
 *
 * The token must have been initialized (token_init / token_set) or filled by
 * the lexer, a token holding garbage is not safe to free.
 *
 * @param[in,out] token Token to free.
 */
void token_free(t_token *token);

/**
 * @brief Zeroes the whole token (str_val = NULL).
 *
 * The parser calls this once on its current token before the first
 * lexer_next() call. It does NOT free anything.
 *
 * @param[out] token Token to initialize.
 */
void token_init(t_token *token);

/**
 * @brief Fills type, line and column and zeroes the value.
 *
 * Scanners call this (through the private emit helper) once the token is
 * complete. It does NOT free anything (lexer_next() already called
 * token_free() on the token). A value such as str_val must be assigned AFTER
 * this call, because it zeroes the value.
 *
 * @param[out] token Token to fill.
 * @param[in] type Type of the token.
 * @param[in] line 1-based line where the token starts.
 * @param[in] column 1-based column where the token starts.
 */
void token_set(t_token *token, t_token_type type, int line, int column);

#endif // TOKEN_H