/**
 * @file lexer.h
 * @brief Lexer interface
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include "token.h"

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

typedef struct {
    FILE *source;
    int current_char;
    bool at_line_start;
} t_lexer;

// ========================================== //
// =============== FUNCTIONS ================ //
// ========================================== //

/**
 * @brief Initializes the lexer with the given source file.
 *
 * @param[in] source Source file to be used by the lexer. It is the caller's responsibility to ensure that the file is opened successfully before passing it to this function.
 * @param[out] lexer Lexer structure to be initialized.
 */
void lexer_init(t_lexer *lexer, FILE *source);

/**
 * @brief Retrieves the next token from the source file.
 *
 * @param[in,out] lexer Pointer to the lexer structure.
 * @param[out] token Token structure to be filled with the next token information.
 *
 * @return 0 on success, 1 on failure.
 */
int lexer_token_advance(t_lexer *lexer, t_token *token);


/**
 * @brief Frees the memory allocated for a token.
 *
 * @param[in,out] token Token structure to be freed.
 */
void token_destroy(t_token *token);

#endif // LEXER_H