/**
 * @file symtable.h
 * @brief Symbol table interface for semantic analysis.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //
#include "ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h> // kvůli SIZE_MAX

// ========================================== //
// ======== CONSTANTS AND TYPEDEFS ========== //
// ========================================== //

typedef size_t t_symbol_id;

#define SYMBOL_ID_INVALID SIZE_MAX

typedef struct t_symtable t_symtable;

typedef struct t_scope t_scope;

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

typedef enum
{
    SYMTABLE_OK = 0,
    SYMTABLE_ERR_ALLOC,
    SYMTABLE_ERR_DUPLICATE,
    SYMTABLE_ERR_NOT_FOUND,
    SYMTABLE_ERR_NO_SCOPE,
    SYMTABLE_ERR_INVALID_ARGUMENT
} t_symtable_result;

typedef enum
{
    FUNCTION_USER,
    FUNCTION_BUILTIN,
    FUNCTION_EXTENSION
} t_function_kind;

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

typedef struct
{
    char *name;

    ENUM_DATA_TYPE type;

    bool is_mutable;
    bool is_parameter;
} t_variable_symbol;

typedef struct
{
    ENUM_DATA_TYPE *types;
    size_t count;
} t_function_parameter_list;

typedef struct
{
    char *name;

    t_function_kind kind;

    ENUM_DATA_TYPE return_type;

    t_function_parameter_list *parameter_lists;
    size_t parameter_list_count;

    ENUM_DATA_TYPE receiver_type;

    bool is_main;
} t_function_symbol;

typedef struct
{
    t_variable_symbol *items;
    size_t count;
    size_t capacity;
} t_variable_pool;

typedef struct
{
    t_function_symbol *items;
    size_t count;
    size_t capacity;
} t_function_pool;

typedef struct
{
    t_variable_pool variables;
    t_function_pool functions;
} t_symbol_pools;
// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

// ----- Lifetime -----
/**
 * Creates and initializes an empty symbol table.
 *
 * The function lookup tree and both symbol pools are initialized empty.
 * No variable scope is created; the current scope is initially NULL.
 *
 * @return Pointer to the newly created symbol table,
 *         or NULL on allocation failure.
 */
t_symtable *symtable_create(void);

/**
 * Destroys the symbol table and all lookup structures still owned by it.
 *
 * All active variable scopes and the function lookup tree are destroyed.
 * Symbol pools are also destroyed unless their ownership was previously
 * transferred using symtable_take_pools().
 *
 * @param table Pointer to the symbol table; may be NULL.
 */
void symtable_destroy(t_symtable *table);

// ----- Scopes -----

/**
 * Creates a new variable scope and makes it the current scope.
 *
 * The new scope uses the previous current scope as its parent.
 * If no scope currently exists, the new scope becomes the root scope.
 *
 * @param table Pointer to the symbol table.
 * @return SYMTABLE_OK on success,
 *         SYMTABLE_ERR_ALLOC on allocation failure,
 *         SYMTABLE_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_symtable_result symtable_enter_scope(t_symtable *table);

/**
 * Destroys the current variable scope and returns to its parent scope.
 *
 * The current scope's variable lookup tree is destroyed. Symbols stored
 * in the variable pool remain valid.
 *
 * @param table Pointer to the symbol table.
 * @return SYMTABLE_OK on success,
 *         SYMTABLE_ERR_NO_SCOPE if no current scope exists,
 *         SYMTABLE_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_symtable_result symtable_leave_scope(t_symtable *table);

/**
 * Temporarily detaches the current variable scope chain from the symbol table.
 *
 * The detached scope chain is not destroyed. The symbol table is left with
 * no active variable scope, so the next call to symtable_enter_scope() creates
 * a new root scope.
 *
 * This is intended for temporarily suspending analysis of one function while
 * another function is analyzed independently.
 *
 * The returned scope must later be restored using symtable_restore_scope().
 *
 * @param table Pointer to the symbol table.
 *
 * @return Pointer to the previously active scope chain,
 *         or NULL if the table is NULL or no scope was active.
 */
t_scope *symtable_detach_scope(t_symtable *table);

/**
 * Restores a previously detached variable scope chain.
 *
 * The symbol table must currently have no active variable scope.
 * The supplied scope becomes the current scope again, preserving its original
 * parent chain.
 *
 * @param table Pointer to the symbol table.
 * @param scope Scope chain previously returned by symtable_detach_scope();
 *              may be NULL to restore an empty scope state.
 *
 * @return SYMTABLE_OK on success,
 *         SYMTABLE_ERR_INVALID_ARGUMENT if the table is NULL
 *         or if another scope is currently active.
 */
t_symtable_result symtable_restore_scope(t_symtable *table, t_scope *scope);

// ----- Variables -----

/**
 * Inserts a variable symbol into the current scope.
 *
 * A new variable ID is assigned, the symbol is stored in the variable pool
 * and its name-to-ID mapping is inserted into the current scope lookup tree.
 *
 * Duplicate names are checked only in the current scope, allowing variables
 * in nested scopes to shadow variables from parent scopes.
 *
 * @param table Pointer to the symbol table.
 * @param symbol Variable symbol to insert.
 * @param out_id Output pointer receiving the assigned variable ID.
 *
 * @return SYMTABLE_OK on success,
 *         SYMTABLE_ERR_DUPLICATE if the name already exists in the current
 * scope, SYMTABLE_ERR_NO_SCOPE if no current scope exists, SYMTABLE_ERR_ALLOC
 * on allocation failure, SYMTABLE_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_symtable_result symtable_insert_variable(t_symtable *table, const t_variable_symbol *symbol,
                                           t_symbol_id *out_id);

/**
 * Finds a variable in the current scope or any of its parent scopes.
 *
 * The search starts in the current scope and continues through parent scopes
 * until the first matching variable is found.
 *
 * @param table Pointer to the symbol table.
 * @param name Name of the variable to find.
 * @param out_id Output pointer receiving the variable ID.
 *
 * @return SYMTABLE_OK if the variable was found,
 *         SYMTABLE_ERR_NOT_FOUND if the variable does not exist
 *         in any accessible scope,
 *         SYMTABLE_ERR_NO_SCOPE if no current scope exists,
 *         SYMTABLE_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_symtable_result symtable_find_variable(const t_symtable *table, const char *name,
                                         t_symbol_id *out_id);

/**
 * Returns a variable symbol stored in the variable pool.
 *
 * The returned pointer remains owned by the symbol table and must not be freed
 * by the caller.
 *
 * @param table Pointer to the symbol table.
 * @param id Variable ID.
 *
 * @return Pointer to the variable symbol if the ID is valid,
 *         or NULL if the table is NULL or the ID does not exist.
 */
t_variable_symbol *symtable_get_variable_by_id(t_symtable *table, t_symbol_id id);

// ----- Functions -----

/**
 * Inserts a function symbol into the function table.
 *
 * A new function ID is assigned, the symbol is stored in the function pool
 * and its lookup key is inserted into the function lookup tree.
 *
 * @param table Pointer to the symbol table.
 * @param symbol Function symbol to insert.
 * @param out_id Output pointer receiving the assigned function ID.
 *
 * @return SYMTABLE_OK on success,
 *         SYMTABLE_ERR_DUPLICATE if the function already exists,
 *         SYMTABLE_ERR_ALLOC on allocation failure,
 *         SYMTABLE_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_symtable_result symtable_insert_function(t_symtable *table, const t_function_symbol *symbol,
                                           t_symbol_id *out_id);

/**
 * Finds a function matching the given name and receiver information.
 *
 * Normal and builtin functions are searched by name. Extension functions
 * are distinguished by their receiver type.
 *
 * @param table Pointer to the symbol table.
 * @param name Name of the function to find.
 * @param has_receiver Whether the searched function has a receiver.
 * @param receiver_type Receiver type used for extension functions.
 * @param out_id Output pointer receiving the function ID.
 *
 * @return SYMTABLE_OK if the function was found,
 *         SYMTABLE_ERR_NOT_FOUND if no matching function exists,
 *         SYMTABLE_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_symtable_result symtable_find_function(const t_symtable *table, const char *name,
                                         bool has_receiver, ENUM_DATA_TYPE receiver_type,
                                         t_symbol_id *out_id);

/**
 * Returns a function symbol stored in the function pool.
 *
 * The returned pointer remains owned by the symbol table and must not be freed
 * by the caller.
 *
 * @param table Pointer to the symbol table.
 * @param id Function ID.
 *
 * @return Pointer to the function symbol if the ID is valid,
 *         or NULL if the table is NULL or the ID does not exist.
 */
t_function_symbol *symtable_get_function_by_id(t_symtable *table, t_symbol_id id);

// ----- Pool ownership transfer -----

/**
 * Transfers ownership of all symbol pools from the symbol table to the caller.
 *
 * After this call, the symbol table no longer owns the returned pools and will
 * not destroy them.
 *
 * @param table Pointer to the symbol table.
 * @return Symbol pools previously owned by the symbol table.
 */
t_symbol_pools symtable_take_pools(t_symtable *table);

/**
 * Destroys all symbol pools and data owned by their symbols.
 *
 * @param pools Pointer to the symbol pools; may be NULL.
 */
void symtable_pools_destroy(t_symbol_pools *pools);

#endif // SYMTABLE_H