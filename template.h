/**
 * @file template.h
 * @brief Stručný popis účelu modulu.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (x274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef TEMPLATE_H // UPRAV FILENAME
#define TEMPLATE_H // UPRAV FILENAME

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

// Hlavičkové soubory projektu:
// #include "other_file.h"

// Standardní knihovny:
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// ========================================== //
// ======== CONSTANTS AND TYPEDEFS ========== //
// ========================================== //

#define PUBLIC_BUFFER_SIZE 128

typedef void (*t_public_callback)(int value);

typedef unsigned int t_data_id;

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

typedef enum
{
    TEMPLATE_OK,
    TEMPLATE_INVALID_PAR,
    TEMPLATE_INTERNAL_ERROR
} t_return_result;

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

typedef struct
{
    t_data_id id;
    char buffer[PUBLIC_BUFFER_SIZE];
    t_public_callback callback;
} t_data;

/*
 * U složitějších struktur jednoduchý popis a graf jejich stavby.
 *
 * Příklad:
 *
 * t_symtable
 * │
 * ├── t_BST *function_tree
 * │       │
 * │       ├── t_BST_node *root
 * │       │       ├── uint32_t height
 * │       │       ├── void *key
 * │       │       ├── void *data
 * │       │       ├── t_destroy_fn destroy_data
 * │       │       ├── t_BST_node *left_child
 * │       │       └── t_BST_node *right_child
 * │       │
 * │       ├── t_compare_fn compare_key
 * │       └── t_destroy_fn destroy_key
 * │
 * └── t_scope *current_scope
 *         │
 *         ├── t_BST *symbol_tree
 *         │       └── ...
 *         │
 *         └── t_scope *parent_scope
 *                 │
 *                 ├── t_BST *symbol_tree
 *                 │       └── ...
 *                 │
 *                 └── t_scope *parent_scope
 *                         └── ...
 */

// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

/**
 * @brief Public funkce používaná jinými moduly.
 *
 * @param[in] a Hodnota ke zpracování.
 *
 * @param[out] result Vyhodnocená data.
 *
 * @return Výsledek funkce.
 * @retval true
 * @retval false
 */
bool public_function(int a, int *result);

#endif // TEMPLATE_H // UPRAV FILENAME