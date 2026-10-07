/**
 * @file lexer.h
 * @brief Rozhraní lexeru
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

#include "token.h"

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

typedef struct {
    FILE *source;
} t_lexer;

// ========================================== //
// =============== FUNCTIONS ================ //
// ========================================== //

/**
 * @brief Initializuje lexer.
 *
 * @param[in] source Zdrojový soubor, ze kterého bude lexer číst. Je zodpovědností volajícího zajistit, že soubor je otevřený a připravený k čtení.
 * @param[out] lexer Lexer, který bude inicializován.
 */
void lexer_init(t_lexer *lexer, FILE *source);

/**
 * @brief Získá další token ze zdrojového souboru.
 *
 * @param[in,out] lexer Ukazatel na strukturu lexeru.
 * @param[out] token Token, do kterého bude uložen získaný token.
 *
 * @return 0 při úspěchu, 1 při lexikální chybě
 */
int lexer_next_token(t_lexer *lexer, t_token *token);


/**
 * @brief Uvolní paměť alokovanou pro token.
 *
 * @param[in,out] token Token, jehož paměť bude uvolněna.
 */
void token_free(t_token *token);

#endif // LEXER_H