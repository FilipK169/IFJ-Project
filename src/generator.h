/**
 * @file generator.h
 * @brief Code generator module interface.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef GENERATOR_H
#define GENERATOR_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "ast.h"

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
