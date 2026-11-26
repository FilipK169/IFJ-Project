/**
 * @file driver.c
 * @brief Driver implementation.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "driver.h"

#include "ast.h"
#include "generator.h"
#include "parser.h"
#include "semantics.h"

#include <stdio.h>
#include <stdlib.h>

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/**
 * @brief Runs lexer and parser and creates AST (Abstrack Syntax Tree).
 *
 * @param[in] in Source code file.
 * @param[out] out_program Abstract syntax tree or NULL.
 *
 * @return Return code of compiler.
 * @retval DRIVER_OK
 * @retval DRIVER_ERR_LEXICAL
 * @retval DRIVER_ERR_SYNTAX
 * @retval DRIVER_ERR_INTERNAL
 */
static int run_parser(FILE *in, AST_PROGRAM **out_program);

/**
 * @brief Runs semantics
 *
 * @param[in,out] program AST from parser
 *
 * @return Return code of compiler
 */
static int run_semantics(AST_PROGRAM *program);

/**
 * @brief Runs code generator
 *
 * @param[in] program AST filled with semantics.
 *
 * @return Return code of compiler.
 * @retval DRIVER_OK
 * @retval DRIVER_ERR_INTERNAL
 */
static int run_generator(AST_PROGRAM *program);

/**
 * @brief Empties stdout and checks write error.
 *
 * @return Return code of compiler.
 * @retval DRIVER_OK
 * @retval DRIVER_ERR_INTERNAL
 */
static int flush_output(void);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static int run_parser(FILE *in, AST_PROGRAM **out_program)
{
    *out_program = NULL;
    int result = (int)parser_parse(in, out_program);

    if (result == DRIVER_OK && *out_program == NULL)
    {
        return DRIVER_ERR_INTERNAL;
    }

    return result;
}

static int run_semantics(AST_PROGRAM *program)
{
    int result = (int)semantics_analyze(program);

    if (result != SEMANTICS_OK)
    {
        fprintf(stderr, "Semantic error (code %d)\n", result);
    }

    return result;
}

static int run_generator(AST_PROGRAM *program)
{
    if (generate_code(program) != 0)
    {
        fprintf(stderr, "Code generation error\n");
        return DRIVER_ERR_INTERNAL;
    }

    return DRIVER_OK;
}

static int flush_output(void)
{
    if (fflush(stdout) != 0 || ferror(stdout))
    {
        fprintf(stderr, "Output write error\n");
        return DRIVER_ERR_INTERNAL;
    }

    return DRIVER_OK;
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

int driver_run(FILE *in)
{
    if (in == NULL)
    {
        return DRIVER_ERR_INTERNAL;
    }

    AST_PROGRAM *program = NULL;
    int result = run_parser(in, &program);

    if (result == DRIVER_OK)
    {
        result = run_semantics(program);
    }

    if (result == DRIVER_OK)
    {
        result = run_generator(program);
    }

    if (result == DRIVER_OK)
    {
        result = flush_output();
    }

    ast_program_destroy(program);
    return result;
}

int main(void)
{
    return driver_run(stdin);
}