/**
 * @file driver.h
 * @brief Driver module of IFJ26 compiler.
 *
 * Runs whole compiler.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Martin Kolouch (xkoloum00)
 */

#ifndef DRIVER_H
#define DRIVER_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include <stdio.h>

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

/**
 * @brief Return codes of compiler
 */
typedef enum
{
    DRIVER_OK = 0,           /**< Succes */
    DRIVER_ERR_LEXICAL = 1,  /**< Lexical error */
    DRIVER_ERR_SYNTAX = 2,   /**< Syntax error */
    DRIVER_ERR_INTERNAL = 99 /**< Internal driver error */
} t_driver_result;

// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

/**
 * @brief Starts compiler and whole proccess - syntax, semantics and code generation
 *
 * @param[in] in Input file with IFJ26 code.
 *
 * @return Return code of compiler
 * @retval DRIVER_OK
 * @retval DRIVER_ERR_LEXICAL
 * @retval DRIVER_ERR_SYNTAX
 * @retval DRIVER_ERR_INTERNAL
 */
int driver_run(FILE *in);

#endif // DRIVER_H
