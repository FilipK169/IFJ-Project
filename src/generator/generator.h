/**
 * @file generator.h
 * @brief Code generator module interface.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef GENERATOR_H // UPRAV FILENAME
#define GENERATOR_H // UPRAV FILENAME

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "../ast/ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

/**
 * @brief Generates code from AST_PROGRAM.
 *
 * @param[in] program AST_PROGRAM to generate code from.
 *
 * @return 0 on success, 1 on error.
 */
int generate_code(AST_PROGRAM *program);

#endif // GENERATOR_H