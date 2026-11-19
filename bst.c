/**
 * @file bst.c
 * @brief Generic balanced binary search tree implementation.
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#include "bst.h"
#include <stdlib.h>

// ============================ //
// ===== STATIC FUNCTIONS ===== //
// ============================ //

static size_t get_height(const t_BST_node *node);
static void update_height(t_BST_node *node);
static int get_balance(const t_BST_node *node);

static void free_node(t_BST *tree, t_BST_node *node);
static void free_subtree(t_BST *tree, t_BST_node *node);

static void rotate_left(t_BST_node **node);
static void rotate_right(t_BST_node **node);
static void rebalance(t_BST_node **node);

static t_BST_node *create_node(void *key, void *data,
                               t_destroy_fn destroy_data);
static t_BST_result find_node(const t_BST *tree, const void *key,
                              t_BST_node **out_node, t_BST_node **out_parent);

static t_BST_node *find_min(t_BST_node *node);
static t_BST_node *find_max(t_BST_node *node);

// ========================== //
// ===== HELPER WORKERS ===== //
// ========================== //
typedef t_BST_result (*t_worker_fn)(t_BST *tree, t_BST_node **node,
                                    void *context);

static t_BST_result walk_modify_subtree(t_BST *tree, t_BST_node **node,
                                        const void *key, void *context,
                                        t_worker_fn worker);

// Insert
typedef struct {
  void *key;
  void *data;
  t_destroy_fn destroy_data;
} t_insert_context;

static t_BST_result insert_worker(t_BST *tree, t_BST_node **node,
                                  void *context);

// Remove
// context = NULL
static t_BST_result remove_worker(t_BST *tree, t_BST_node **node,
                                  void *context);

// ====================== //
// ==== DEFINITIONS ===== //
// ====================== //

/**
 * Returns the stored height of a node.
 *
 * @param node Node whose height should be retrieved.
 * @return Height of the node, or 0 if the node is NULL.
 */
static size_t get_height(const t_BST_node *node) {
  return node ? node->height : 0;
}

/**
 * Recalculates and updates the height of a node based on the heights
 * of its left and right subtrees.
 *
 * @param node Node whose height should be updated.
 */
static void update_height(t_BST_node *node) {
  if (node) {
    size_t left_height = get_height(node->left_child);
    size_t right_height = get_height(node->right_child);
    node->height =
        ((left_height > right_height ? left_height : right_height) + 1);
  }
}

/**
 * Returns the balance factor of a node.
 *
 * @param node Node whose balance factor should be calculated.
 * @return Balance factor of the node, or 0 if the node is NULL.
 */
static int get_balance(const t_BST_node *node) {
  if (!node)
    return 0;
  return (int)get_height(node->left_child) - (int)get_height(node->right_child);
}

/**
 * Frees a single node, including its key and data if applicable.
 *
 * @param tree Pointer to the binary search tree.
 * @param node Node to be freed.
 */
static void free_node(t_BST *tree, t_BST_node *node) {
  if (!tree || !node)
    return;
  if (node->destroy_data && node->data) {
    node->destroy_data(node->data);
  }
  if (tree->destroy_key && node->key) {
    tree->destroy_key(node->key);
  }
  free(node);
}

/**
 * Recursively frees a subtree rooted at the given node.
 *
 * @param tree Pointer to the binary search tree.
 * @param node Root of the subtree to be freed.
 */
static void free_subtree(t_BST *tree, t_BST_node *node) {
  if (!tree || !node)
    return;
  free_subtree(tree, node->left_child);
  free_subtree(tree, node->right_child);
  free_node(tree, node);
}

/**
 * Performs a left rotation on the given node.
 *
 * @param node Pointer to the pointer holding the subtree root.
 */
static void rotate_left(t_BST_node **node) {
  /*
     y                x
    / \              / \
   T1  x     ->     y  T3
      / \          / \
     T2 T3        T1 T2
  */
  t_BST_node *y = *node;
  t_BST_node *x = y->right_child;
  t_BST_node *T2 = x->left_child;

  // Perform rotation
  x->left_child = y;
  y->right_child = T2;

  // Update heights
  update_height(y);
  update_height(x);

  // Update root
  *node = x;
}

/**
 * Performs a right rotation on the given node.
 *
 * @param node Pointer to the pointer holding the subtree root.
 */
static void rotate_right(t_BST_node **node) {
  /*
       y              x
      / \            / \
     x   T3   ->    T1  y
    / \                / \
   T1 T2              T2 T3
  */
  t_BST_node *y = *node;
  t_BST_node *x = y->left_child;
  t_BST_node *T2 = x->right_child;

  // Perform rotation
  x->right_child = y;
  y->left_child = T2;

  // Update heights
  update_height(y);
  update_height(x);

  // Update root
  *node = x;
}

/**
 * Rebalances the given node if it is unbalanced.
 *
 * @param node Pointer to the pointer holding the subtree root.
 */
static void rebalance(t_BST_node **node) {
  if (!node || !*node)
    return;

  update_height(*node);
  int balance = get_balance(*node);

  // Left heavy
  if (balance > 1) {
    if (get_balance((*node)->left_child) < 0) {
      rotate_left(&(*node)->left_child);
    }
    rotate_right(node);
  }
  // Right heavy
  else if (balance < -1) {
    if (get_balance((*node)->right_child) > 0) {
      rotate_right(&(*node)->right_child);
    }
    rotate_left(node);
  }
}

/**
 * Creates a new binary search tree node with the given key and data.
 *
 * @param key Pointer to the key for the new node.
 * @param data Pointer to the data for the new node.
 * @param destroy_data Function pointer to destroy the data when the node is
 * freed.
 * @return Pointer to the newly created node, or NULL on allocation failure.
 */
static t_BST_node *create_node(void *key, void *data,
                               t_destroy_fn destroy_data) {
  t_BST_node *new_node = malloc(sizeof(t_BST_node));
  if (!new_node)
    return NULL;

  new_node->key = key;
  new_node->data = data;
  new_node->destroy_data = destroy_data;
  new_node->left_child = NULL;
  new_node->right_child = NULL;
  new_node->height = 1; // New node is initially added at leaf

  return new_node;
}

/**
 * Finds a node with the given key in the tree and optionally returns its
 parent.
 *
 * @param tree Pointer to the binary search tree.
 * @param key Pointer to the key to search for.
 * @param out_node Output pointer receiving the found node; may be NULL if not
 required.
 * @param out_parent Output pointer receiving the parent of the found node,
                  or the last visited node if the key is not found;
                  may be NULL if not required.
 * @return BST_OK if the node was found,
 *         BST_ERR_NOT_FOUND if the key does not exist,
 *         BST_ERR_INVALID_ARGUMENT if the arguments are invalid.
 */
static t_BST_result find_node(const t_BST *tree, const void *key,
                              t_BST_node **out_node, t_BST_node **out_parent) {
  if (!tree || !key) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  t_BST_node *current = tree->root;
  t_BST_node *parent = NULL;

  while (current) {
    int cmp = tree->compare_key(key, current->key);
    if (cmp == 0) {
      if (out_node)
        *out_node = current;
      if (out_parent)
        *out_parent = parent;
      return BST_OK;
    }
    parent = current;
    current = (cmp < 0) ? current->left_child : current->right_child;
  }

  if (out_node)
    *out_node = NULL;
  if (out_parent)
    *out_parent = parent;
  return BST_ERR_NOT_FOUND;
}

/**
 * Finds the node with the minimum key in the given subtree.
 *
 * @param node Pointer to the root of the subtree.
 * @return Pointer to the node with the minimum key, or NULL if the subtree is
 * empty.
 */
static t_BST_node *find_min(t_BST_node *node) {
  if (!node)
    return NULL;
  while (node->left_child) {
    node = node->left_child;
  }
  return node;
}

/**
 * Finds the node with the maximum key in the given subtree.
 *
 * @param node Pointer to the root of the subtree.
 * @return Pointer to the node with the maximum key, or NULL if the subtree is
 * empty.
 */
static t_BST_node *find_max(t_BST_node *node) {
  if (!node)
    return NULL;
  while (node->right_child) {
    node = node->right_child;
  }
  return node;
}

// ========================== //
// ===== HELPER WORKERS ===== //
// ========================== //

/**
 * Recursively walks through the tree toward the position of the given key,
 * calls the supplied worker function on the matching node or empty position,
 * and rebalances affected subtrees while returning from the recursion.
 *
 * @param tree Pointer to the binary search tree.
 * @param node Pointer to the pointer holding the current subtree root.
 * @param key Key used to determine the traversal direction.
 * @param context Optional operation-specific context passed to the worker.
 * @param worker Function called on the matching node or empty position.
 * @return Result returned by the worker or recursive operation.
 */
static t_BST_result walk_modify_subtree(t_BST *tree, t_BST_node **node,
                                        const void *key, void *context,
                                        t_worker_fn worker) {
  if (!tree || !node || !key || !worker) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  t_BST_result result = BST_ERR_NOT_FOUND;
  if (!*node) {
    // If the current node is NULL, call the worker function
    result = worker(tree, node, context);
    rebalance(node);
    return result;
  }

  int cmp = tree->compare_key(key, (*node)->key);
  if (cmp < 0) {
    result =
        walk_modify_subtree(tree, &(*node)->left_child, key, context, worker);
  } else if (cmp > 0) {
    result =
        walk_modify_subtree(tree, &(*node)->right_child, key, context, worker);
  } else {
    // Key matches existing node, call the worker function on the current node
    result = worker(tree, node, context);
  }

  if (result == BST_OK)
    rebalance(node);

  return result;
}

/**
 * Inserts a new node at the supplied empty tree position.
 *
 * The insertion parameters are provided through the operation context.
 * If the position already contains a node, the key is considered a duplicate.
 *
 * @param tree Pointer to the binary search tree.
 * @param node Pointer to the pointer holding the target tree position.
 * @param context Pointer to a t_insert_context containing the key, data,
 *                and data destructor for the new node.
 * @return BST_OK on successful insertion,
 *         BST_ERR_DUPLICATE_KEY if the position is already occupied,
 *         BST_ERR_ALLOC if node allocation fails,
 *         or BST_ERR_INVALID_ARGUMENT for invalid input.
 */
static t_BST_result insert_worker(t_BST *tree, t_BST_node **node,
                                  void *context) {
  if (!tree || !node || !context) {
    return BST_ERR_INVALID_ARGUMENT;
  }
  t_insert_context *ctx = (t_insert_context *)context;

  if (!*node) {
    // Create a new node if the current position is NULL
    *node = create_node(ctx->key, ctx->data, ctx->destroy_data);
    if (!*node) {
      return BST_ERR_ALLOC; // Memory allocation failure
    }
    tree->size++;
    return BST_OK;
  }
  return BST_ERR_DUPLICATE_KEY; // Key already exists
}

/**
 * Removes the node at the supplied tree position.
 *
 * For a leaf, the node is removed directly. For a node with one child,
 * the node is replaced by its child. For a node with two children,
 * the replacement is selected from the taller subtree using either the
 * maximum node of the left subtree or the minimum node of the right subtree.
 * The payloads are exchanged and the replacement node is then removed
 * recursively.
 *
 * @param tree Pointer to the binary search tree.
 * @param node Pointer to the pointer holding the node to remove.
 * @param context Unused operation context.
 * @return BST_OK on successful removal,
 *         BST_ERR_NOT_FOUND if the target position is empty,
 *         or BST_ERR_INVALID_ARGUMENT for invalid input.
 */
static t_BST_result remove_worker(t_BST *tree, t_BST_node **node,
                                  void *context) {
  (void)context;
  if (!tree || !node) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  // NOT FOUND CASE
  if (!*node) {
    return BST_ERR_NOT_FOUND;
  }

  // 0 CHILDREN CASE
  if (!(*node)->left_child && !(*node)->right_child) {
    // Simply remove the node
    free_node(tree, *node);
    *node = NULL;
    tree->size--;
    return BST_OK;
  }

  // 1 CHILD CASE
  if (!(*node)->left_child || !(*node)->right_child) {
    // Replace the node with its child
    t_BST_node *target = *node;
    *node = target->left_child ? target->left_child : target->right_child;
    free_node(tree, target);
    tree->size--;

    return BST_OK;
  }

  // 2 CHILDREN CASE
  t_BST_node *replacement_node = NULL;
  if (get_height((*node)->left_child) >= get_height((*node)->right_child)) {
    // Left subtree is taller or equal, find the maximum in the left subtree
    replacement_node = find_max((*node)->left_child);
    void *temp_key = (*node)->key;
    void *temp_data = (*node)->data;
    t_destroy_fn temp_destroy_data = (*node)->destroy_data;

    (*node)->key = replacement_node->key;
    (*node)->data = replacement_node->data;
    (*node)->destroy_data = replacement_node->destroy_data;

    replacement_node->key = temp_key;
    replacement_node->data = temp_data;
    replacement_node->destroy_data = temp_destroy_data;

    return walk_modify_subtree(tree, &(*node)->left_child, replacement_node->key,
                               NULL, remove_worker);
  } else {
    // Right subtree is taller, find the minimum in the right subtree
    replacement_node = find_min((*node)->right_child);
    void *temp_key = (*node)->key;
    void *temp_data = (*node)->data;
    t_destroy_fn temp_destroy_data = (*node)->destroy_data;

    (*node)->key = replacement_node->key;
    (*node)->data = replacement_node->data;
    (*node)->destroy_data = replacement_node->destroy_data;

    replacement_node->key = temp_key;
    replacement_node->data = temp_data;
    replacement_node->destroy_data = temp_destroy_data;

    return walk_modify_subtree(tree, &(*node)->right_child,
                               replacement_node->key, NULL, remove_worker);
  }
}

// ============================ //
// ===== PUBLIC FUNCTIONS ===== //
// ============================ //

t_BST *BST_create(t_compare_fn compare_key, t_copy_fn copy_key,
                  t_destroy_fn destroy_key) {
  if (compare_key == NULL) {
    return NULL; // Invalid argument
  }

  t_BST *tree = malloc(sizeof(t_BST));
  if (!tree) {
    return NULL; // Memory allocation failure
  }

  tree->root = NULL;
  tree->size = 0;
  tree->compare_key = compare_key;
  tree->copy_key = copy_key;
  tree->destroy_key = destroy_key;

  return tree;
}

t_BST_result BST_insert(t_BST *tree, void *key, void *data,
                        t_destroy_fn destroy_data) {
  if (!tree || !key) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  return walk_modify_subtree(tree, &tree->root, key,
                             &(t_insert_context){key, data, destroy_data},
                             insert_worker);
}

t_BST_result BST_insert_copy(t_BST *tree, const void *key, const void *data,
                             t_copy_fn copy_data, t_destroy_fn destroy_data) {
  if (!tree || !key || !tree->copy_key || !tree->destroy_key ||
      (data && !copy_data) || (data && !destroy_data)) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  void *owned_key = tree->copy_key(key);
  if (!owned_key) {
    return BST_ERR_ALLOC;
  }

  void *owned_data = NULL;
  if (data) {
    owned_data = copy_data(data);
    if (!owned_data) {
      tree->destroy_key(owned_key);
      return BST_ERR_ALLOC;
    }
  }

  t_BST_result result = walk_modify_subtree(
      tree, &tree->root, owned_key,
      &(t_insert_context){owned_key, owned_data, destroy_data}, insert_worker);
  if (result != BST_OK) {
    tree->destroy_key(owned_key);
    if (owned_data)
      destroy_data(owned_data);
  }
  return result;
}

t_BST_result BST_remove(t_BST *tree, const void *key) {
  if (!tree || !key) {
    return BST_ERR_INVALID_ARGUMENT;
  }
  return walk_modify_subtree(tree, &tree->root, key, NULL, remove_worker);
}

t_BST_result BST_extract(t_BST *tree, const void *key, void **out_data,
                         t_destroy_fn *out_destroy_data) {
  if (!tree || !key) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  t_BST_node *found_node = NULL;
  t_BST_result result = find_node(tree, key, &found_node, NULL);
  if (result == BST_OK) {
    if (out_data) {
      *out_data = found_node->data;
    } else {
      if (found_node->destroy_data && found_node->data) {
        found_node->destroy_data(found_node->data);
      }
    }
    found_node->data = NULL;
    if (out_destroy_data)
      *out_destroy_data = found_node->destroy_data;
    found_node->destroy_data = NULL;
    if (!out_data && out_destroy_data)
      *out_destroy_data = NULL;
    return walk_modify_subtree(tree, &tree->root, key, NULL, remove_worker);
  }
  if (out_data)
    *out_data = NULL;
  if (out_destroy_data)
    *out_destroy_data = NULL;
  return result;
}

t_BST_result BST_find(const t_BST *tree, const void *key, void **out_data) {
  if (!tree || !key || !out_data) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  t_BST_node *found_node = NULL;
  t_BST_result result = find_node(tree, key, &found_node, NULL);
  *out_data = (result == BST_OK) ? found_node->data : NULL;
  return result;
}

bool BST_contains(const t_BST *tree, const void *key) {
  if (!tree || !key) {
    return false;
  }

  return find_node(tree, key, NULL, NULL) == BST_OK;
}

t_BST_result BST_min(const t_BST *tree, const void **out_key, void **out_data) {
  if (!tree) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  t_BST_node *found_node = find_min(tree->root);
  if (out_data)
    *out_data = (found_node) ? found_node->data : NULL;
  if (out_key)
    *out_key = (found_node) ? found_node->key : NULL;
  return (found_node) ? BST_OK : BST_ERR_NOT_FOUND;
}

t_BST_result BST_max(const t_BST *tree, const void **out_key, void **out_data) {
  if (!tree) {
    return BST_ERR_INVALID_ARGUMENT;
  }

  t_BST_node *found_node = find_max(tree->root);
  if (out_data)
    *out_data = (found_node) ? found_node->data : NULL;
  if (out_key)
    *out_key = (found_node) ? found_node->key : NULL;
  return (found_node) ? BST_OK : BST_ERR_NOT_FOUND;
}

size_t BST_node_count(const t_BST *tree) {
  if (!tree) {
    return 0;
  }
  return tree->size;
}

void BST_clear(t_BST *tree) {
  if (!tree) {
    return;
  }
  free_subtree(tree, tree->root);
  tree->root = NULL;
  tree->size = 0;
}

void BST_destroy(t_BST *tree) {
  BST_clear(tree);
  free(tree);
}