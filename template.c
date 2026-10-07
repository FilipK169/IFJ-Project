/**
 * @file template.c
 * @brief Stručný popis implementace modulu.
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

// Vlastní header vždy jako první:
#include "template.h"

// Další hlavičkové soubory projektu:
//#include "other_file.h"

// Standardní knihovny např:
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ========================================== //
// ===== PRIVATE CONSTANTS AND TYPEDEFS ===== //
// ========================================== //

#define PRIVATE_BUFFER_SIZE 128

typedef unsigned int t_private_data_id;

typedef void (*t_private_callback)(int value);

// ========================================== //
// ============ PRIVATE ENUMS =============== //
// ========================================== //

typedef enum { PRIVATE_OK, PRIVATE_INVALID_PAR, PRIVATE_INTERNAL_ERROR } t_private_return_result;

// ========================================== //
// ========== PRIVATE STRUCTURES ============ //
// ========================================== //

typedef struct {
  t_private_data_id id;
  char buffer[PRIVATE_BUFFER_SIZE];
  t_private_callback callback;
} t_private_data;

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/**
 * @brief Pomocná funkce používaná pouze uvnitř tohoto modulu.
 *
 * @param[in] value Hodnota ke zpracování.
 *
 * @param[out] data Vyhodnocená data.
 *
 * @return Výsledek funkce.
 * @retval PRIVATE_OK - popis
 * @retval PRIVATE_INVALID_PAR - popis
 * @retval PRIVATE_INTERNAL_ERROR - popis
 */
static t_private_return_result helper_function(int value, t_private_data *data);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/*
 * AI use
 * Prompt: abc...
 */
static t_private_return_result helper_function(int value, t_private_data *data) {
  (void)value;
  (void)data;
  // ...
  return PRIVATE_OK;
}
/*
 * END AI
 */

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

/*
 * AI use
 * Prompt: abc...
 */
bool public_function(int a, int *result) {
  (void)a;
  (void)result;
  helper_function(a, NULL);
  // ...
  return true;
}
/*
 * END AI
 */
