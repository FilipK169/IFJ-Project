/**
 * @file parser_internal.h
 * @brief Private helpers shared between the parser .c files of IFJ26.
 *
 * Only the parser .c files include this header, everybody else includes
 * parser.h.
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

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

/** Parser state: the lexer and the token we are currently looking at. */
typedef struct {
    t_lexer lexer;
    t_token cur;
} t_parser;

// ========================================== //
// ===== DECLARATION OF PRIVATE FUNCTIONS === //
// ========================================== //

/** @brief Moves to the next token; the previous token (and its string) is released. */
t_parse_rc parser_advance(t_parser *p);

#endif // PARSER_INTERNAL_H