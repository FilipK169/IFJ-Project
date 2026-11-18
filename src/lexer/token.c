/**
 * @file token.c
 * @brief Token helpers for the IFJ26 lexer.
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

#include "token.h"

#include <stdlib.h>
#include <string.h>

// ========================================== //
// ============ PUBLIC FUNCTIONS ============ //
// ========================================== //

const char *token_type_name(t_token_type type) {
    switch (type) {
    case TOKEN_EOF:
        return "EOF";

    // Identifiers and literals
    case TOKEN_ID:
        return "ID";
    case TOKEN_INT_LIT:
        return "INT_LIT";
    case TOKEN_DOUBLE_LIT:
        return "DOUBLE_LIT";
    case TOKEN_STRING_LIT:
        return "STRING_LIT";

    // Keywords
    case TOKEN_KW_DEF:
        return "KW_DEF";
    case TOKEN_KW_DO:
        return "KW_DO";
    case TOKEN_KW_DOUBLE:
        return "KW_DOUBLE";
    case TOKEN_KW_ELSE:
        return "KW_ELSE";
    case TOKEN_KW_IF:
        return "KW_IF";
    case TOKEN_KW_IMPORT:
        return "KW_IMPORT";
    case TOKEN_KW_INT:
        return "KW_INT";
    case TOKEN_KW_RETURN:
        return "KW_RETURN";
    case TOKEN_KW_STRING:
        return "KW_STRING";
    case TOKEN_KW_THEN:
        return "KW_THEN";
    case TOKEN_KW_UNIT:
        return "KW_UNIT";
    case TOKEN_KW_VAL:
        return "KW_VAL";
    case TOKEN_KW_VAR:
        return "KW_VAR";
    case TOKEN_KW_WHILE:
        return "KW_WHILE";

    // Keywords from extensions
    case TOKEN_KW_TRUE:
        return "KW_TRUE";
    case TOKEN_KW_FALSE:
        return "KW_FALSE";
    case TOKEN_KW_BOOLEAN:
        return "KW_BOOLEAN";
    case TOKEN_KW_FOR:
        return "KW_FOR";

    // Operators
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_MULTIPLY:
        return "MULTIPLY";
    case TOKEN_DIVIDE:
        return "DIVIDE";
    case TOKEN_LESS:
        return "LESS";
    case TOKEN_LESS_EQUAL:
        return "LESS_EQUAL";
    case TOKEN_GREATER:
        return "GREATER";
    case TOKEN_GREATER_EQUAL:
        return "GREATER_EQUAL";
    case TOKEN_EQUAL:
        return "EQUAL";
    case TOKEN_NOT_EQUAL:
        return "NOT_EQUAL";

    // Operators from extensions
    case TOKEN_NOT:
        return "NOT";
    case TOKEN_AND:
        return "AND";
    case TOKEN_OR:
        return "OR";

    // Syntax
    case TOKEN_ASSIGN:
        return "ASSIGN";
    case TOKEN_COLON:
        return "COLON";
    case TOKEN_DOT:
        return "DOT";
    case TOKEN_LEFT_PAREN:
        return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
        return "RIGHT_PAREN";
    case TOKEN_COMMA:
        return "COMMA";
    case TOKEN_SEMICOLON:
        return "SEMICOLON";
    case TOKEN_ANNOT_MAIN:
        return "ANNOT_MAIN";
    case TOKEN_LEFT_ARROW:
        return "LEFT_ARROW";
    case TOKEN_UNION:
        return "UNION";

    // Interpolated strings
    case TOKEN_ISTR_BEGIN:
        return "ISTR_BEGIN";
    case TOKEN_ISTR_TEXT:
        return "ISTR_TEXT";
    case TOKEN_ISTR_ID:
        return "ISTR_ID";
    case TOKEN_ISTR_EXPR_BEGIN:
        return "ISTR_EXPR_BEGIN";
    case TOKEN_ISTR_EXPR_END:
        return "ISTR_EXPR_END";
    case TOKEN_ISTR_END:
        return "ISTR_END";

    // Indentation
    case TOKEN_EOL:
        return "EOL";
    case TOKEN_INDENT:
        return "INDENT";
    case TOKEN_DEDENT:
        return "DEDENT";
    }

    // enum value is added and not handled above.
    return "UNKNOWN";
}

int token_has_string(t_token_type type) {
    switch (type) {
        case TOKEN_ID:
        case TOKEN_STRING_LIT:
        case TOKEN_ISTR_TEXT:
        case TOKEN_ISTR_ID:
            return 1;
        default:
            return 0;
    }
}

void token_free(t_token *token) {
    if (token == NULL) {
        return;
    }
    if (token_has_string(token->type)) {
        free(token->value.str_val);
        token->value.str_val = NULL;
    }
}

void token_init(t_token *token) {
    memset(token, 0, sizeof(*token));
    // memset gives str_val == NULL on every platform we target; be explicit anyway.
    token->value.str_val = NULL;
}

void token_set(t_token *token, t_token_type type, int line, int column) {
    memset(token, 0, sizeof(*token));
    token->value.str_val = NULL;
    token->type = type;
    token->line = line;
    token->column = column;
}