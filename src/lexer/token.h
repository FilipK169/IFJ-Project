/**
 * @file token.h
 * @brief Token interface
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
// ================ ENUMS =================== //
// ========================================== //

typedef enum {
    TOKEN_EOF,

    // Identifiers and literals
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_DOUBLE,
    TOKEN_STRING,

    // Keywords
    TOKEN_DEF,
    TOKEN_DO,
    TOKEN_DOUBLE_TYPE,
    TOKEN_ELSE,
    TOKEN_IF,
    TOKEN_IMPORT,
    TOKEN_INT_TYPE,
    TOKEN_RETURN,
    TOKEN_STRING_TYPE,
    TOKEN_THEN,
    TOKEN_UNIT,
    TOKEN_VAL,
    TOKEN_VAR,
    TOKEN_WHILE,

    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_NOT_EQUAL,

    // Syntax
    TOKEN_ASSIGN,
    TOKEN_COLON,
    TOKEN_AT,
    TOKEN_DOT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,

    // Indentation
    TOKEN_EOL,
    TOKEN_INDENT,
    TOKEN_DEDENT

} t_token_type;

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

typedef struct {
    t_token_type type;
    char *lexeme;
    bool at_line_start;
} t_token;

typedef struct {
    const char *text;
    t_token_type type;
} t_token_match;

#endif // TOKEN_H