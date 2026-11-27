/**
 * @file symtable.c
 * @brief Symbol table implementation for semantic analysis.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 */

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "symtable.h"
#include "bst.h"

#include <stdlib.h>
#include <string.h>

// ========================================== //
// ========== PRIVATE STRUCTURES ============ //
// ========================================== //

typedef struct t_scope
{
    struct t_scope *parent_scope;
    t_BST *variable_tree;
} t_scope;

typedef struct
{
    const char *name;
    bool has_receiver;
    ENUM_DATA_TYPE receiver_type;
} t_function_key;

struct t_symtable
{
    t_BST *function_tree;
    t_scope *current_scope;
    t_symbol_pools pools;
};

// ========================================== //
// ============ STATIC FUNCTIONS ============ //
// ========================================== //

// BST callbacks

// Compares variable names used as keys in variable lookup trees.
static int compare_string(const void *first, const void *second);

// Creates an owned copy of a variable name for the BST key.
static void *copy_string(const void *data);

// Creates an owned copy of a symbol ID stored as BST data.
static void *copy_symbol_id(const void *data);

// Compares function lookup keys: name, receiver presence and receiver type.
static int compare_function_key(const void *first, const void *second);

// Creates a deep copy of a function lookup key for the BST.
static void *copy_function_key(const void *data);

// Destroys a function lookup key, including its copied name.
static void destroy_function_key(void *data);

// Scopes

// Creates a new scope with the given parent and an empty variable lookup tree.
static t_scope *scope_create(t_scope *parent_scope);

// Destroys one scope and its variable lookup tree.
static void scope_destroy(t_scope *scope);

// Finds a variable only in the current scope.
// Used internally for duplicate checking during insertion.
static t_symtable_result find_variable_current(const t_symtable *table, const char *name,
                                               t_symbol_id *out_id);

// Pools

// Ensures the variable pool can hold at least the requested number of symbols.
static bool variable_pool_reserve(t_variable_pool *pool, size_t capacity);

// Ensures the function pool can hold at least the requested number of symbols.
static bool function_pool_reserve(t_function_pool *pool, size_t capacity);

// Symbol management

// Creates a deep copy of a variable symbol, including its name.
static bool variable_symbol_copy(t_variable_symbol *destination, const t_variable_symbol *source);

// Destroys all dynamically allocated data owned by a variable symbol.
static void variable_symbol_destroy(t_variable_symbol *symbol);

// Creates a deep copy of a function symbol, including its name and parameter
// lists.
static bool function_symbol_copy(t_function_symbol *destination, const t_function_symbol *source);

// Destroys all dynamically allocated data owned by a function symbol.
static void function_symbol_destroy(t_function_symbol *symbol);

// Creates a deep copy of all parameter lists and their type arrays.
static bool parameter_lists_copy(t_function_parameter_list **out_lists,
                                 const t_function_parameter_list *lists, size_t count);

// Destroys all parameter lists and their type arrays.
static void parameter_lists_destroy(t_function_parameter_list *lists, size_t count);

// ========================================== //
// ============== DEFINITIONS =============== //
// ========================================== //

// ----- BST callbacks -----

static int compare_string(const void *first, const void *second)
{
    return strcmp((const char *)first, (const char *)second);
}

static void *copy_string(const void *data)
{
    if (!data)
        return NULL;

    size_t length = strlen((const char *)data) + 1;
    char *copy = malloc(length);

    if (!copy)
        return NULL;

    strcpy(copy, (const char *)data);
    return copy;
}

static void *copy_symbol_id(const void *data)
{
    if (!data)
        return NULL;

    size_t size = sizeof(t_symbol_id);
    t_symbol_id *copy = malloc(size);

    if (!copy)
        return NULL;

    memcpy(copy, data, size);
    return copy;
}

static int compare_function_key(const void *first, const void *second)
{
    const t_function_key *key1 = (const t_function_key *)first;
    const t_function_key *key2 = (const t_function_key *)second;

    int name_cmp = strcmp(key1->name, key2->name);
    if (name_cmp != 0)
    {
        return name_cmp;
    }

    if (key1->has_receiver != key2->has_receiver)
    {
        return key1->has_receiver ? 1 : -1;
    }

    if (key1->has_receiver)
    {
        if (key1->receiver_type != key2->receiver_type)
        {
            return (int)key1->receiver_type - (int)key2->receiver_type;
        }
    }

    return 0;
}

static void *copy_function_key(const void *data)
{
    if (!data)
        return NULL;

    const t_function_key *source = data;

    t_function_key *copy = malloc(sizeof(t_function_key));
    if (!copy)
        return NULL;

    copy->name = copy_string(source->name);
    if (!copy->name)
    {
        free(copy);
        return NULL;
    }

    copy->has_receiver = source->has_receiver;
    copy->receiver_type = source->receiver_type;

    return copy;
}

static void destroy_function_key(void *data)
{
    if (!data)
        return;

    t_function_key *key = data;
    free((void *)key->name);
    free(key);
}

// ----- Scopes -----

static t_scope *scope_create(t_scope *parent_scope)
{
    t_scope *scope = malloc(sizeof(t_scope));
    if (!scope)
        return NULL;

    scope->parent_scope = parent_scope;
    scope->variable_tree = BST_create(compare_string, copy_string, free);
    if (!scope->variable_tree)
    {
        free(scope);
        return NULL;
    }

    return scope;
}

static void scope_destroy(t_scope *scope)
{
    if (!scope)
        return;

    BST_destroy(scope->variable_tree);
    free(scope);
}

static t_symtable_result find_variable_current(const t_symtable *table, const char *name,
                                               t_symbol_id *out_id)
{

    if (!table || !name || !out_id)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    if (!table->current_scope)
        return SYMTABLE_ERR_NO_SCOPE;

    void *data = NULL;

    t_BST_result result = BST_find(table->current_scope->variable_tree, name, &data);

    if (result == BST_ERR_NOT_FOUND)
        return SYMTABLE_ERR_NOT_FOUND;

    if (result != BST_OK)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    *out_id = *(t_symbol_id *)data;

    return SYMTABLE_OK;
}

// ----- Pools -----

static bool variable_pool_reserve(t_variable_pool *pool, size_t capacity)
{

    if (!pool)
        return false;

    if (capacity <= pool->capacity)
        return true;

    if (capacity > SIZE_MAX / sizeof(t_variable_symbol))
        return false;

    t_variable_symbol *new_items = realloc(pool->items, capacity * sizeof(t_variable_symbol));

    if (!new_items)
        return false;

    pool->items = new_items;
    pool->capacity = capacity;

    return true;
}

static bool function_pool_reserve(t_function_pool *pool, size_t capacity)
{
    if (!pool)
        return false;

    if (capacity <= pool->capacity)
        return true;

    if (capacity > SIZE_MAX / sizeof(t_function_symbol))
        return false;

    t_function_symbol *new_items = realloc(pool->items, capacity * sizeof(t_function_symbol));

    if (!new_items)
        return false;

    pool->items = new_items;
    pool->capacity = capacity;

    return true;
}

// ----- Symbol management -----

static bool variable_symbol_copy(t_variable_symbol *destination, const t_variable_symbol *source)
{

    if (!destination || !source)
        return false;

    destination->name = copy_string(source->name);
    if (!destination->name)
        return false;

    destination->type = source->type;
    destination->is_mutable = source->is_mutable;
    destination->is_parameter = source->is_parameter;

    return true;
}

static void variable_symbol_destroy(t_variable_symbol *symbol)
{
    if (!symbol)
        return;

    free(symbol->name);
    symbol->name = NULL;
}

static bool function_symbol_copy(t_function_symbol *destination, const t_function_symbol *source)
{
    if (!destination || !source)
        return false;

    destination->name = copy_string(source->name);
    if (!destination->name)
        return false;

    destination->kind = source->kind;
    destination->return_type = source->return_type;
    destination->receiver_type = source->receiver_type;
    destination->is_main = source->is_main;

    destination->parameter_lists = NULL;
    destination->parameter_list_count = 0;

    if (!parameter_lists_copy(&destination->parameter_lists, source->parameter_lists,
                              source->parameter_list_count))
    {
        free(destination->name);
        destination->name = NULL;
        return false;
    }

    destination->parameter_list_count = source->parameter_list_count;

    return true;
}

static void function_symbol_destroy(t_function_symbol *symbol)
{
    if (!symbol)
        return;

    free(symbol->name);
    symbol->name = NULL;

    parameter_lists_destroy(symbol->parameter_lists, symbol->parameter_list_count);

    symbol->parameter_lists = NULL;
    symbol->parameter_list_count = 0;
}

static bool parameter_lists_copy(t_function_parameter_list **out_lists,
                                 const t_function_parameter_list *lists, size_t count)
{
    if (!out_lists)
        return false;

    *out_lists = NULL;

    if (count == 0)
        return true;

    if (!lists)
        return false;

    if (count > SIZE_MAX / sizeof(t_function_parameter_list))
        return false;

    t_function_parameter_list *copy = malloc(count * sizeof(t_function_parameter_list));

    if (!copy)
        return false;

    for (size_t i = 0; i < count; i++)
    {
        copy[i].types = NULL;
        copy[i].count = 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        copy[i].count = lists[i].count;

        if (lists[i].count == 0)
            continue;

        if (!lists[i].types)
        {
            parameter_lists_destroy(copy, count);
            return false;
        }

        if (lists[i].count > SIZE_MAX / sizeof(ENUM_DATA_TYPE))
        {
            parameter_lists_destroy(copy, count);
            return false;
        }

        copy[i].types = malloc(lists[i].count * sizeof(ENUM_DATA_TYPE));

        if (!copy[i].types)
        {
            parameter_lists_destroy(copy, count);
            return false;
        }

        memcpy(copy[i].types, lists[i].types, lists[i].count * sizeof(ENUM_DATA_TYPE));
    }

    *out_lists = copy;
    return true;
}

static void parameter_lists_destroy(t_function_parameter_list *lists, size_t count)
{
    if (!lists)
        return;

    for (size_t i = 0; i < count; i++)
    {
        free(lists[i].types);
        lists[i].types = NULL;
        lists[i].count = 0;
    }

    free(lists);
}

// ========================================== //
// ============ PUBLIC FUNCTIONS ============ //
// ========================================== //

t_symtable *symtable_create(void)
{
    t_symtable *table = malloc(sizeof(t_symtable));
    if (!table)
        return NULL;

    table->function_tree =
        BST_create(compare_function_key, copy_function_key, destroy_function_key);

    if (!table->function_tree)
    {
        free(table);
        return NULL;
    }

    table->current_scope = NULL;

    table->pools.variables.items = NULL;
    table->pools.variables.count = 0;
    table->pools.variables.capacity = 0;

    table->pools.functions.items = NULL;
    table->pools.functions.count = 0;
    table->pools.functions.capacity = 0;

    return table;
}

void symtable_destroy(t_symtable *table)
{
    if (!table)
        return;

    // Free all scopes
    while (table->current_scope)
    {
        t_scope *parent = table->current_scope->parent_scope;
        scope_destroy(table->current_scope);
        table->current_scope = parent;
    }

    BST_destroy(table->function_tree);

    symtable_pools_destroy(&table->pools);

    free(table);
}

t_symtable_result symtable_enter_scope(t_symtable *table)
{
    if (!table)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    t_scope *new_scope = scope_create(table->current_scope);
    if (!new_scope)
        return SYMTABLE_ERR_ALLOC;

    table->current_scope = new_scope;

    return SYMTABLE_OK;
}

t_symtable_result symtable_leave_scope(t_symtable *table)
{
    if (!table)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    if (!table->current_scope)
        return SYMTABLE_ERR_NO_SCOPE;

    t_scope *old_scope = table->current_scope;
    table->current_scope = old_scope->parent_scope;

    scope_destroy(old_scope);
    return SYMTABLE_OK;
}

t_scope *symtable_detach_scope(t_symtable *table)
{
    if (!table)
        return NULL;

    t_scope *scope = table->current_scope;
    table->current_scope = NULL;

    return scope;
}

t_symtable_result symtable_restore_scope(t_symtable *table, t_scope *scope)
{
    if (!table)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    if (table->current_scope)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    table->current_scope = scope;

    return SYMTABLE_OK;
}

t_symtable_result symtable_insert_variable(t_symtable *table, const t_variable_symbol *symbol,
                                           t_symbol_id *out_id)
{
    if (!table || !symbol || !symbol->name || !out_id)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    if (!table->current_scope)
        return SYMTABLE_ERR_NO_SCOPE;

    t_symbol_id existing_id;
    t_symtable_result result = find_variable_current(table, symbol->name, &existing_id);

    if (result == SYMTABLE_OK)
        return SYMTABLE_ERR_DUPLICATE;

    if (result != SYMTABLE_ERR_NOT_FOUND)
        return result;

    t_variable_pool *pool = &table->pools.variables;

    if (pool->count == SIZE_MAX)
        return SYMTABLE_ERR_ALLOC;

    if (!variable_pool_reserve(pool, pool->count + 1))
        return SYMTABLE_ERR_ALLOC;

    t_symbol_id id = pool->count;

    if (!variable_symbol_copy(&pool->items[id], symbol))
        return SYMTABLE_ERR_ALLOC;

    t_BST_result bst_result = BST_insert_copy(table->current_scope->variable_tree, symbol->name,
                                              &id, copy_symbol_id, free);

    if (bst_result != BST_OK)
    {
        variable_symbol_destroy(&pool->items[id]);

        // shouldnt happen, but just in case
        if (bst_result == BST_ERR_DUPLICATE_KEY)
            return SYMTABLE_ERR_DUPLICATE;

        if (bst_result == BST_ERR_ALLOC)
            return SYMTABLE_ERR_ALLOC;

        return SYMTABLE_ERR_INVALID_ARGUMENT;
    }

    pool->count++;
    *out_id = id;

    return SYMTABLE_OK;
}

t_symtable_result symtable_find_variable(const t_symtable *table, const char *name,
                                         t_symbol_id *out_id)
{
    if (!table || !name || !out_id)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    if (!table->current_scope)
        return SYMTABLE_ERR_NO_SCOPE;
    t_scope *scope = table->current_scope;

    while (scope)
    {
        void *data = NULL;

        t_BST_result bst_result = BST_find(scope->variable_tree, name, &data);

        if (bst_result == BST_OK)
        {
            *out_id = *(t_symbol_id *)data;
            return SYMTABLE_OK;
        }

        if (bst_result != BST_ERR_NOT_FOUND)
            return SYMTABLE_ERR_INVALID_ARGUMENT;

        scope = scope->parent_scope;
    }

    return SYMTABLE_ERR_NOT_FOUND;
}

t_variable_symbol *symtable_get_variable_by_id(t_symtable *table, t_symbol_id id)
{
    if (!table)
        return NULL;

    t_variable_pool *pool = &table->pools.variables;
    if (id >= pool->count)
        return NULL;
    return &pool->items[id];
}

t_symtable_result symtable_insert_function(t_symtable *table, const t_function_symbol *symbol,
                                           t_symbol_id *out_id)
{
    if (!table || !symbol || !symbol->name || !out_id)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    bool has_receiver = symbol->kind == FUNCTION_EXTENSION;

    t_symbol_id existing_id;
    t_symtable_result result = symtable_find_function(table, symbol->name, has_receiver,
                                                      symbol->receiver_type, &existing_id);

    if (result == SYMTABLE_OK)
        return SYMTABLE_ERR_DUPLICATE;

    if (result != SYMTABLE_ERR_NOT_FOUND)
        return result;

    t_function_pool *pool = &table->pools.functions;

    if (pool->count == SIZE_MAX)
        return SYMTABLE_ERR_ALLOC;

    if (!function_pool_reserve(pool, pool->count + 1))
        return SYMTABLE_ERR_ALLOC;

    t_symbol_id id = pool->count;

    if (!function_symbol_copy(&pool->items[id], symbol))
        return SYMTABLE_ERR_ALLOC;

    t_function_key key = {
        .name = symbol->name,
        .has_receiver = has_receiver,
        .receiver_type = symbol->receiver_type,
    };

    t_BST_result bst_result =
        BST_insert_copy(table->function_tree, &key, &id, copy_symbol_id, free);

    if (bst_result != BST_OK)
    {
        function_symbol_destroy(&pool->items[id]);

        if (bst_result == BST_ERR_DUPLICATE_KEY)
            return SYMTABLE_ERR_DUPLICATE;

        if (bst_result == BST_ERR_ALLOC)
            return SYMTABLE_ERR_ALLOC;

        return SYMTABLE_ERR_INVALID_ARGUMENT;
    }

    pool->count++;
    *out_id = id;

    return SYMTABLE_OK;
}

t_symtable_result symtable_find_function(const t_symtable *table, const char *name,
                                         bool has_receiver, ENUM_DATA_TYPE receiver_type,
                                         t_symbol_id *out_id)
{
    if (!table || !name || !out_id)
        return SYMTABLE_ERR_INVALID_ARGUMENT;

    t_function_key key = {
        .name = name,
        .has_receiver = has_receiver,
        .receiver_type = receiver_type,
    };

    void *data = NULL;
    t_BST_result bst_result = BST_find(table->function_tree, &key, &data);

    if (bst_result == BST_OK)
    {
        *out_id = *(t_symbol_id *)data;
        return SYMTABLE_OK;
    }

    if (bst_result == BST_ERR_NOT_FOUND)
        return SYMTABLE_ERR_NOT_FOUND;

    return SYMTABLE_ERR_INVALID_ARGUMENT;
}

t_function_symbol *symtable_get_function_by_id(t_symtable *table, t_symbol_id id)
{
    if (!table)
        return NULL;

    t_function_pool *pool = &table->pools.functions;
    if (id >= pool->count)
        return NULL;

    return &pool->items[id];
}

t_symbol_pools symtable_take_pools(t_symtable *table)
{
    t_symbol_pools pools = {0};

    if (!table)
        return pools;

    pools.variables = table->pools.variables;
    pools.functions = table->pools.functions;

    table->pools.variables.items = NULL;
    table->pools.variables.count = 0;
    table->pools.variables.capacity = 0;

    table->pools.functions.items = NULL;
    table->pools.functions.count = 0;
    table->pools.functions.capacity = 0;

    return pools;
}

void symtable_pools_destroy(t_symbol_pools *pools)
{
    if (!pools)
        return;

    for (size_t i = 0; i < pools->variables.count; i++)
    {
        variable_symbol_destroy(&pools->variables.items[i]);
    }
    free(pools->variables.items);
    pools->variables.items = NULL;
    pools->variables.count = 0;
    pools->variables.capacity = 0;

    for (size_t i = 0; i < pools->functions.count; i++)
    {
        function_symbol_destroy(&pools->functions.items[i]);
    }
    free(pools->functions.items);
    pools->functions.items = NULL;
    pools->functions.count = 0;
    pools->functions.capacity = 0;
}