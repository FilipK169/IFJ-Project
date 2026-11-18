/**
 * @file parser.c
 * @brief Recursive descent parser of IFJ26 (tokens -> AST).
 *
 * Step 1: only the skeleton. It reads all tokens up to the end of the file,
 * so lexical errors are reported, but it builds an empty program.
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

#include "parser_internal.h"

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/** Lexer result codes and parser result codes share the same numbers, but are separate types. */
static t_parse_rc lex_rc(t_lex_rc rc) {
    switch (rc) {
    case LEX_OK:
        return PARSE_OK;
    case LEX_ERR_LEXICAL:
        return PARSE_ERR_LEXICAL;
    default:
        return PARSE_ERR_INTERNAL;
    }
}

t_parse_rc parser_advance(t_parser *p) {
    // lexer_next frees the previous content of p->cur by itself
    return lex_rc(lexer_next(&p->lexer, &p->cur));
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

t_parse_rc parser_parse(FILE *in, AST_PROGRAM **out_program) {
    if (in == NULL || out_program == NULL) {
        return PARSE_ERR_INTERNAL;
    }
    *out_program = NULL;

    t_parser p;
    t_parse_rc rc = lex_rc(lexer_init(&p.lexer, in));
    if (rc != PARSE_OK) {
        return rc;
    }
    token_init(&p.cur); // lexer_next frees the old token first, so it must start zeroed

    AST_PROGRAM *program = ast_program_create();
    if (program == NULL) {
        lexer_destroy(&p.lexer);
        return PARSE_ERR_INTERNAL;
    }

    // Step 1 only: walk through all tokens until EOF
    rc = parser_advance(&p);
    while (rc == PARSE_OK && p.cur.type != TOKEN_EOF) {
        rc = parser_advance(&p);
    }

    token_free(&p.cur);
    lexer_destroy(&p.lexer);

    if (rc != PARSE_OK) {
        ast_program_destroy(program);
        return rc;
    }
    *out_program = program;
    return PARSE_OK;
}