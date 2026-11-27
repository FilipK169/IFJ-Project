/**
 * @file parser.h
 * @brief Syntax analyzer (parser) public interface for IFJ26.
 *
 * Recursive descent parser. It pulls tokens from the lexer and builds the AST
 * described in ast.h. It does NOT touch the symbol table: everything marked
 * "Semantics fill" in ast.h (ids, resolved types, ...) is left untouched.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Ondřej Vondra (x274921)
 */

#ifndef PARSER_H
#define PARSER_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "ast.h"

#include <stdio.h>

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

/**
 * @brief Return codes of the parser.
 *
 * The numeric values are the compiler exit codes (same as the lexer ones).
 */
typedef enum
{
    PARSE_OK = 0,           // success, *out_program holds the tree
    PARSE_ERR_LEXICAL = 1,  // lexical error (reported by the lexer)
    PARSE_ERR_SYNTAX = 2,   // syntax error (message already printed)
    PARSE_ERR_INTERNAL = 99 // malloc failed, internal bug
} t_parse_rc;

// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

/**
 * @brief Parses a whole IFJ26 program read from `in`.
 *
 * Creates and owns its own lexer (does NOT close `in`). On success the caller
 * owns the tree and releases it with ast_program_destroy(). On failure nothing
 * is leaked and *out_program is NULL. A message was already printed to stderr.
 *
 * Semantic things (missing / duplicate @main, redefinitions, types, ...) are
 * NOT checked here.
 *
 * @param[in] in Input stream with the source code.
 * @param[out] out_program Receives the parsed program.
 *
 * @return Result code.
 * @retval PARSE_OK - program parsed
 * @retval PARSE_ERR_LEXICAL - lexical error (exit code 1)
 * @retval PARSE_ERR_SYNTAX - syntax error (exit code 2)
 * @retval PARSE_ERR_INTERNAL - memory allocation failed / internal bug (exit code 99)
 */
t_parse_rc parser_parse(FILE *in, AST_PROGRAM **out_program);

#endif // PARSER_H