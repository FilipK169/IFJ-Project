/**
 * @file bst.h
 * @brief Generic balanced binary search tree interface.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef BINSEARCHTREE_H
#define BINSEARCHTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Pointer to a function used to destroy dynamically allocated data.
 */
typedef void (*t_destroy_fn)(void *data);

/**
 * Pointer to a function used to compare two keys.
 *
 * Returns:
 * < 0 if first < second
 * = 0 if first == second
 * > 0 if first > second
 */
typedef int (*t_compare_fn)(const void *first, const void *second);

/**
 * Pointer to a function used to create a copy of an object.
 *
 * @param data Pointer to the object to copy.
 * @return Pointer to the newly allocated copy, NULL on failure.
 */
typedef void *(*t_copy_fn)(const void *data);

/**
 * Node of the binary search tree.
 */
typedef struct t_BST_node
{
    size_t height;

    void *key;
    void *data;

    t_destroy_fn destroy_data;

    struct t_BST_node *left_child;
    struct t_BST_node *right_child;
} t_BST_node;

/**
 * Binary search tree.
 */
typedef struct t_BST
{
    t_BST_node *root;

    size_t size;

    t_compare_fn compare_key;
    t_copy_fn copy_key;
    t_destroy_fn destroy_key;
} t_BST;

/**
 * Possible results of binary search tree operations.
 */
typedef enum t_BST_result
{
    BST_OK = 0,
    BST_ERR_ALLOC,
    BST_ERR_DUPLICATE_KEY,
    BST_ERR_NOT_FOUND,
    BST_ERR_INVALID_ARGUMENT,
    BST_ERR_MISSING_FUNCTION
} t_BST_result;

/**
 * Creates an empty binary search tree.
 *
 * @param compare_key Function used to compare two keys.
 * @param copy_key Function used to create a copy of a key; may be NULL if
 *                 key copying is not required.
 * @param destroy_key Function used to destroy keys; may be NULL if keys
 *                    must not be destroyed by the tree.
 * @return Pointer to the newly created tree, NULL on allocation failure.
 */
t_BST *BST_create(t_compare_fn compare_key, t_copy_fn copy_key, t_destroy_fn destroy_key);

/**
 * Inserts a key-data pair into the tree WITHOUT copying the provided data.
 *
 * The tree stores the provided pointers directly. If destroy_data is not NULL,
 * it will be called when the data is removed from the tree.
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key.
 * @param data Pointer to the data to store.
 * @param destroy_data Function used to destroy the stored data; may be NULL.
 * @return BST_OK on success,
 *         BST_ERR_DUPLICATE_KEY if the key already exists,
 *         BST_ERR_ALLOC on allocation failure,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_insert(t_BST *tree, void *key, void *data, t_destroy_fn destroy_data);

/**
 * Inserts a key-data pair into the tree by creating copies of both.
 *
 * The key is copied using the tree's copy_key function and the data is copied
 * using copy_data.
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key to copy.
 * @param data Pointer to the data to copy.
 * @param copy_data Function used to create a copy of the data.
 * @param destroy_data Function used to destroy the copied data.
 * @return BST_OK on success,
 *         BST_ERR_DUPLICATE_KEY if the key already exists,
 *         BST_ERR_ALLOC if copying or allocation fails,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_insert_copy(t_BST *tree, const void *key, const void *data, t_copy_fn copy_data,
                             t_destroy_fn destroy_data);

/**
 * Removes an item from the tree and destroys its key and data.
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key identifying the item to remove.
 * @return BST_OK on success,
 *         BST_ERR_NOT_FOUND if the key does not exist,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_remove(t_BST *tree, const void *key);

/**
 * Removes an item from the tree without destroying its stored data.
 *
 * Ownership of the returned data is transferred to the caller.
 *
 * if out_data is NULL it destroys them
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key identifying the item to extract.
 * @param out_data Output pointer receiving the stored data.
 * @param out_destroy_data Output pointer receiving the destructor associated
 *                         with the data; may be NULL if not required.
 * @return BST_OK on success,
 *         BST_ERR_NOT_FOUND if the key does not exist,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_extract(t_BST *tree, const void *key, void **out_data,
                         t_destroy_fn *out_destroy_data);

/**
 * Finds data associated with a key.
 *
 * The returned data remains owned by the tree.
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key to search for.
 * @param out_data Output pointer receiving the stored data.
 * @return BST_OK if the key was found,
 *         BST_ERR_NOT_FOUND if the key does not exist,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_find(const t_BST *tree, const void *key, void **out_data);

/**
 * Checks whether a key exists in the tree.
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key to search for.
 * @return true if the key exists, false otherwise.
 */
bool BST_contains(const t_BST *tree, const void *key);

/**
 * Finds the minimum key in the tree and retrieves its associated data.
 *
 * @param tree Pointer to the binary search tree.
 * @param out_key Output pointer receiving the minimum key.
 * @param out_data Output pointer receiving the associated data.
 * @return BST_OK if the tree is not empty,
 *         BST_ERR_NOT_FOUND if the tree is empty,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_min(const t_BST *tree, const void **out_key, void **out_data);

/**
 * Finds the maximum key in the tree and retrieves its associated data.
 *
 * @param tree Pointer to the binary search tree.
 * @param out_key Output pointer receiving the maximum key.
 * @param out_data Output pointer receiving the associated data.
 * @return BST_OK if the tree is not empty,
 *         BST_ERR_NOT_FOUND if the tree is empty,
 *         BST_ERR_INVALID_ARGUMENT for invalid arguments.
 */
t_BST_result BST_max(const t_BST *tree, const void **out_key, void **out_data);

/**
 * Returns the number of nodes stored in the tree.
 *
 * @param tree Pointer to the binary search tree.
 * @return Number of stored nodes; 0 if tree is NULL.
 */
size_t BST_node_count(const t_BST *tree);

/**
 * Removes all nodes from the tree.
 *
 * Stored keys and data are destroyed using their respective destroy functions.
 * The tree itself remains allocated and can be reused.
 *
 * @param tree Pointer to the binary search tree.
 */
void BST_clear(t_BST *tree);

/**
 * Destroys the entire binary search tree.
 *
 * All nodes, keys and owned data are destroyed before the tree itself is freed.
 *
 * @param tree Pointer to the binary search tree.
 */
void BST_destroy(t_BST *tree);

#endif // BINSEARCHTREE_H