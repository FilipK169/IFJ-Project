/**
 * @file semantics.h
 * @brief Semantic analysis interface.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef SEMANTICS_H
#define SEMANTICS_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "ast.h"
#include "symtable.h"

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

/**
 * @brief Possible results of semantic analysis.
 *
 * Numeric values correspond directly to the IFJ26 compiler
 * semantic error exit codes.
 */
typedef enum
{
    SEMANTICS_OK = 0,

    SEMANTICS_ERR_UNDEFINED = 3,
    SEMANTICS_ERR_REDEFINITION = 4,
    SEMANTICS_ERR_FUNCTION_ARGS = 5,
    SEMANTICS_ERR_TYPE = 6,
    SEMANTICS_ERR_IMMUTABLE = 7,
    SEMANTICS_ERR_OTHER = 10,

    SEMANTICS_ERR_INTERNAL = 99
} t_semantics_result;

// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

/**
 * @brief Performs semantic analysis of the whole program.
 *
 * The analysis modifies the supplied AST in place. On successful analysis,
 * semantic information such as symbol IDs, resolved data types, implicit
 * conversions and resolved function/member references is written into
 * the corresponding AST nodes.
 *
 * The resulting variable and function symbol pools are transferred to
 * the caller through out_pools. The pools remain valid after semantic
 * analysis finishes and may be used by later compiler stages such as
 * code generation.
 *
 * The AST remains owned by the caller.
 *
 * On success, the caller takes ownership of the returned symbol pools
 * and must eventually destroy them using symtable_pools_destroy().
 *
 * On failure, out_pools is left empty.
 *
 * @param[in,out] program Parsed program represented by the AST.
 * @param[out] out_pools Output structure receiving the resulting symbol pools.
 *
 * @return SEMANTICS_OK on success,
 *         SEMANTICS_ERR_UNDEFINED for undefined symbols or missing @main,
 *         SEMANTICS_ERR_REDEFINITION for invalid symbol redefinition,
 *         SEMANTICS_ERR_FUNCTION_ARGS for invalid function arguments
 *         or @main parameters,
 *         SEMANTICS_ERR_TYPE for type incompatibility or failed type inference,
 *         SEMANTICS_ERR_IMMUTABLE for assignment to an immutable variable
 *         or function parameter,
 *         SEMANTICS_ERR_OTHER for other semantic errors,
 *         SEMANTICS_ERR_INTERNAL for internal compiler errors.
 */
t_semantics_result semantics_analyze(AST_PROGRAM *program);

#endif // SEMANTICS_H
