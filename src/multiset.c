/*
 * Copyright (c) 2017-2019 Bailey Thompson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <errno.h>
#include "include/multiset.h"

struct internal_multiset {
    size_t key_size;
    int (*comparator)(const void *const one, const void *const two);
    int size;
    struct node *root;
};

struct node {
    int count;
    struct node *parent;
    int balance;
    void *key;
    struct node *left;
    struct node *right;
};

/**
 * Initializes a multi-set.
 *
 * @param key_size   the size of each element in the multi-set; must be
 *                   positive
 * @param comparator the comparator function used for key ordering; must not be
 *                   NULL
 *
 * @return the newly-initialized multi-set, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
multiset multiset_init(const size_t key_size,
                       int (*const comparator)(const void *const,
                                               const void *const))
{
    struct internal_multiset *init;
    if (key_size == 0 || !comparator) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_multiset));
    if (!init) {
        return NULL;
    }
    init->key_size = key_size;
    init->comparator = comparator;
    init->size = 0;
    init->root = NULL;
    return init;
}

/**
 * Gets the size of the multi-set.
 *
 * @param me the multi-set to check
 *
 * @return the size of the multi-set
 */
int multiset_size(multiset me)
{
    return me->size;
}

/**
 * Determines whether or not the multi-set is empty.
 *
 * @param me the multi-set to check
 *
 * @return 1 if the multi-set is empty, otherwise 0
 */
int multiset_is_empty(multiset me)
{
    return multiset_size(me) == 0;
}

/*
 * Resets the parent reference.
 */
static void multiset_reference_parent(multiset me,
                                      struct node *const parent,
                                      struct node *const child)
{
    child->parent = parent->parent;
    if (!parent->parent) {
        me->root = child;
    } else if (parent->parent->left == parent) {
        parent->parent->left = child;
    } else {
        parent->parent->right = child;
    }
}

/*
 * Rotates the AVL tree to the left.
 */
static void multiset_rotate_left(multiset me,
                                 struct node *const parent,
                                 struct node *const child)
{
    struct node *grand_child;
    multiset_reference_parent(me, parent, child);
    grand_child = child->left;
    if (grand_child) {
        grand_child->parent = parent;
    }
    parent->parent = child;
    parent->right = grand_child;
    child->left = parent;
}

/*
 * Rotates the AVL tree to the right.
 */
static void multiset_rotate_right(multiset me,
                                  struct node *const parent,
                                  struct node *const child)
{
    struct node *grand_child;
    multiset_reference_parent(me, parent, child);
    grand_child = child->right;
    if (grand_child) {
        grand_child->parent = parent;
    }
    parent->parent = child;
    parent->left = grand_child;
    child->right = parent;
}

/*
 * Performs a left repair.
 */
static struct node *multiset_repair_left(multiset me,
                                         struct node *const parent,
                                         struct node *const child)
{
    multiset_rotate_left(me, parent, child);
    if (child->balance == 0) {
        parent->balance = 1;
        child->balance = -1;
    } else {
        parent->balance = 0;
        child->balance = 0;
    }
    return child;
}

/*
 * Performs a right repair.
 */
static struct node *multiset_repair_right(multiset me,
                                          struct node *const parent,
                                          struct node *const child)
{
    multiset_rotate_right(me, parent, child);
    if (child->balance == 0) {
        parent->balance = -1;
        child->balance = 1;
    } else {
        parent->balance = 0;
        child->balance = 0;
    }
    return child;
}

/*
 * Performs a left-right repair.
 */
static struct node *multiset_repair_left_right(multiset me,
                                               struct node *const parent,
                                               struct node *const child,
                                               struct node *const grand_child)
{
    multiset_rotate_left(me, child, grand_child);
    multiset_rotate_right(me, parent, grand_child);
    if (grand_child->balance == 1) {
        parent->balance = 0;
        child->balance = -1;
    } else if (grand_child->balance == 0) {
        parent->balance = 0;
        child->balance = 0;
    } else {
        parent->balance = 1;
        child->balance = 0;
    }
    grand_child->balance = 0;
    return grand_child;
}

/*
 * Performs a right-left repair.
 */
static struct node *multiset_repair_right_left(multiset me,
                                               struct node *const parent,
                                               struct node *const child,
                                               struct node *const grand_child)
{
    multiset_rotate_right(me, child, grand_child);
    multiset_rotate_left(me, parent, grand_child);
    if (grand_child->balance == 1) {
        parent->balance = -1;
        child->balance = 0;
    } else if (grand_child->balance == 0) {
        parent->balance = 0;
        child->balance = 0;
    } else {
        parent->balance = 0;
        child->balance = 1;
    }
    grand_child->balance = 0;
    return grand_child;
}

/*
 * Repairs the AVL tree on insert. The only possible values of parent->balance
 * are {-2, 2} and the only possible values of child->balance are {-1, 0, 1}.
 */
static struct node *multiset_repair(multiset me,
                                    struct node *const parent,
                                    struct node *const child,
                                    struct node *const grand_child)
{
    if (parent->balance == 2) {
        if (child->balance == -1) {
            return multiset_repair_right_left(me, parent, child, grand_child);
        }
        return multiset_repair_left(me, parent, child);
    }
    if (child->balance == 1) {
        return multiset_repair_left_right(me, parent, child, grand_child);
    }
    return multiset_repair_right(me, parent, child);
}

/*
 * Balances the AVL tree on insert.
 */
static void multiset_insert_balance(multiset me, struct node *const item)
{
    struct node *grand_child = NULL;
    struct node *child = item;
    struct node *parent = item->parent;
    while (parent) {
        if (parent->left == child) {
            parent->balance--;
        } else {
            parent->balance++;
        }
        /* If balance is zero after modification, then the tree is balanced. */
        if (parent->balance == 0) {
            return;
        }
        /* Must re-balance if not in {-1, 0, 1} */
        if (parent->balance > 1 || parent->balance < -1) {
            /* After one repair, the tree is balanced. */
            multiset_repair(me, parent, child, grand_child);
            return;
        }
        grand_child = child;
        child = parent;
        parent = parent->parent;
    }
}

/*
 * Creates and allocates a node.
 */
static struct node *multiset_create_node(multiset me,
                                         const void *const data,
                                         struct node *const parent)
{
    struct node *const insert = malloc(sizeof(struct node));
    if (!insert) {
        return NULL;
    }
    insert->count = 1;
    insert->parent = parent;
    insert->balance = 0;
    insert->key = malloc(me->key_size);
    if (!insert->key) {
        free(insert);
        return NULL;
    }
    memcpy(insert->key, data, me->key_size);
    insert->left = NULL;
    insert->right = NULL;
    me->size++;
    return insert;
}

/**
 * Adds a key to the multi-set. The pointer to the key being passed in should
 * point to the key type which this multi-set holds. For example, if this
 * multi-set holds key integers, the key pointer should be a pointer to an
 * integer. Since the key is being copied, the pointer only has to be valid
 * when this function is called.
 *
 * @param me  the multi-set to add to
 * @param key the key to add
 *
 * @return 0       if no error
 * @return -ENOMEM if out of memory
 */
int multiset_put(multiset me, void *const key)
{
    struct node *traverse;
    if (!me->root) {
        struct node *insert = multiset_create_node(me, key, NULL);
        if (!insert) {
            return -ENOMEM;
        }
        me->root = insert;
        return 0;
    }
    traverse = me->root;
    for (;;) {
        const int compare = me->comparator(key, traverse->key);
        if (compare < 0) {
            if (traverse->left) {
                traverse = traverse->left;
            } else {
                struct node *insert = multiset_create_node(me, key, traverse);
                if (!insert) {
                    return -ENOMEM;
                }
                traverse->left = insert;
                multiset_insert_balance(me, insert);
                return 0;
            }
        } else if (compare > 0) {
            if (traverse->right) {
                traverse = traverse->right;
            } else {
                struct node *insert = multiset_create_node(me, key, traverse);
                if (!insert) {
                    return -ENOMEM;
                }
                traverse->right = insert;
                multiset_insert_balance(me, insert);
                return 0;
            }
        } else {
            traverse->count++;
            me->size++;
            return 0;
        }
    }
}

/*
 * If a match occurs, returns the match. Else, returns NULL.
 */
static struct node *multiset_equal_match(multiset me, const void *const key)
{
    struct node *traverse = me->root;
    if (!traverse) {
        return 0;
    }
    for (;;) {
        const int compare = me->comparator(key, traverse->key);
        if (compare < 0) {
            if (traverse->left) {
                traverse = traverse->left;
            } else {
                return NULL;
            }
        } else if (compare > 0) {
            if (traverse->right) {
                traverse = traverse->right;
            } else {
                return NULL;
            }
        } else {
            return traverse;
        }
    }
}

/**
 * Determines the count of a specific key in the multi-set. The pointer to the
 * key being passed in should point to the key type which this multi-set holds.
 * For example, if this multi-set holds key integers, the key pointer should be
 * a pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the multi-set to check for the count
 * @param key the key to check
 *
 * @return the count of a specific key in the multi-set
 */
int multiset_count(multiset me, void *const key)
{
    const struct node *const item = multiset_equal_match(me, key);
    if (!item) {
        return 0;
    }
    return item->count;
}

/**
 * Determines if the multi-set contains the specified key. The pointer to the
 * key being passed in should point to the key type which this multi-set holds.
 * For example, if this multi-set holds key integers, the key pointer should be
 * a pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the multi-set to check for the key
 * @param key the key to check
 *
 * @return 1 if the multiset contained the key, otherwise 0
 */
int multiset_contains(multiset me, void *const key)
{
    return multiset_equal_match(me, key) != NULL;
}

/*
 * Repairs the AVL tree by pivoting on an item.
 */
static struct node *multiset_repair_pivot(multiset me,
                                          struct node *const item,
                                          const int is_left_pivot)
{
    struct node *const child = is_left_pivot ? item->right : item->left;
    struct node *const grand_child =
            child->balance == 1 ? child->right : child->left;
    return multiset_repair(me, item, child, grand_child);
}

/*
 * Goes back up the tree repairing it along the way.
 */
static void multiset_trace_ancestors(multiset me, struct node *item)
{
    struct node *child = item;
    struct node *parent = item->parent;
    while (parent) {
        if (parent->left == child) {
            parent->balance++;
        } else {
            parent->balance--;
        }
        /* The tree is balanced if balance is -1 or +1 after modification. */
        if (parent->balance == -1 || parent->balance == 1) {
            return;
        }
        /* Must re-balance if not in {-1, 0, 1} */
        if (parent->balance > 1 || parent->balance < -1) {
            child = multiset_repair_pivot(me, parent, parent->left == child);
            parent = child->parent;
            /* If balance is -1 or +1 after modification or the parent is */
            /* NULL, then the tree is balanced. */
            if (!parent || child->balance == -1 || child->balance == 1) {
                return;
            }
        } else {
            child = parent;
            parent = parent->parent;
        }
    }
}

/*
 * Balances the AVL tree on deletion.
 */
static void multiset_delete_balance(multiset me,
                                    struct node *item,
                                    const int is_left_deleted)
{
    if (is_left_deleted) {
        item->balance++;
    } else {
        item->balance--;
    }
    /* If balance is -1 or +1 after modification, then the tree is balanced. */
    if (item->balance == -1 || item->balance == 1) {
        return;
    }
    /* Must re-balance if not in {-1, 0, 1} */
    if (item->balance > 1 || item->balance < -1) {
        item = multiset_repair_pivot(me, item, is_left_deleted);
        if (!item->parent || item->balance == -1 || item->balance == 1) {
            return;
        }
    }
    multiset_trace_ancestors(me, item);
}

/*
 * Removes traverse when it has no children.
 */
static void multiset_remove_no_children(multiset me,
                                        const struct node *const traverse)
{
    struct node *const parent = traverse->parent;
    /* If no parent and no children, then the only node is traverse. */
    if (!parent) {
        me->root = NULL;
        return;
    }
    /* No re-reference needed since traverse has no children. */
    if (parent->left == traverse) {
        parent->left = NULL;
        multiset_delete_balance(me, parent, 1);
    } else {
        parent->right = NULL;
        multiset_delete_balance(me, parent, 0);
    }
}

/*
 * Removes traverse when it has one child.
 */
static void multiset_remove_one_child(multiset me,
                                      const struct node *const traverse)
{
    struct node *const parent = traverse->parent;
    /* If no parent, make the child of traverse the new root. */
    if (!parent) {
        if (traverse->left) {
            traverse->left->parent = NULL;
            me->root = traverse->left;
        } else {
            traverse->right->parent = NULL;
            me->root = traverse->right;
        }
        return;
    }
    /* The parent of traverse now references the child of traverse. */
    if (parent->left == traverse) {
        if (traverse->left) {
            parent->left = traverse->left;
            traverse->left->parent = parent;
        } else {
            parent->left = traverse->right;
            traverse->right->parent = parent;
        }
        multiset_delete_balance(me, parent, 1);
    } else {
        if (traverse->left) {
            parent->right = traverse->left;
            traverse->left->parent = parent;
        } else {
            parent->right = traverse->right;
            traverse->right->parent = parent;
        }
        multiset_delete_balance(me, parent, 0);
    }
}

/*
 * Removes traverse when it has two children.
 */
static void multiset_remove_two_children(multiset me,
                                         const struct node *const traverse)
{
    struct node *item;
    struct node *parent;
    const int is_left_deleted = traverse->right->left != NULL;
    if (!is_left_deleted) {
        item = traverse->right;
        parent = item;
        item->balance = traverse->balance;
        item->parent = traverse->parent;
        item->left = traverse->left;
        item->left->parent = item;
    } else {
        item = traverse->right->left;
        while (item->left) {
            item = item->left;
        }
        parent = item->parent;
        item->balance = traverse->balance;
        item->parent->left = item->right;
        if (item->right) {
            item->right->parent = item->parent;
        }
        item->left = traverse->left;
        item->left->parent = item;
        item->right = traverse->right;
        item->right->parent = item;
        item->parent = traverse->parent;
    }
    if (!traverse->parent) {
        me->root = item;
    } else if (traverse->parent->left == traverse) {
        item->parent->left = item;
    } else {
        item->parent->right = item;
    }
    multiset_delete_balance(me, parent, is_left_deleted);
}

/*
 * Removes the element from the set.
 */
static void multiset_remove_element(multiset me, struct node *const traverse)
{
    if (!traverse->left && !traverse->right) {
        multiset_remove_no_children(me, traverse);
    } else if (!traverse->left || !traverse->right) {
        multiset_remove_one_child(me, traverse);
    } else {
        multiset_remove_two_children(me, traverse);
    }
    free(traverse->key);
    free(traverse);
}

/**
 * Removes a key from the multi-set if it contains it. The pointer to the key
 * being passed in should point to the key type which this multi-set holds. For
 * example, if this multi-set holds key integers, the key pointer should be a
 * pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the multi-set to remove a key from
 * @param key the key to remove
 *
 * @return 1 if the multi-set contained the key, otherwise 0
 */
int multiset_remove(multiset me, void *const key)
{
    struct node *const traverse = multiset_equal_match(me, key);
    if (!traverse) {
        return 0;
    }
    traverse->count--;
    if (traverse->count == 0) {
        multiset_remove_element(me, traverse);
    }
    me->size--;
    return 1;
}

/**
 * Removes all the occurrences of a specified key in the multi-set. The pointer
 * to the key being passed in should point to the key type which this multi-set
 * holds. For example, if this multi-set holds key integers, the key pointer
 * should be a pointer to an integer. Since the key is being copied, the pointer
 * only has to be valid when this function is called.
 *
 * @param me  the multi-set to remove a key from
 * @param key the key to remove
 *
 * @return 1 if the multi-set contained the key, otherwise 0
 */
int multiset_remove_all(multiset me, void *const key)
{
    struct node *const traverse = multiset_equal_match(me, key);
    if (!traverse) {
        return 0;
    }
    me->size -= traverse->count;
    multiset_remove_element(me, traverse);
    return 1;
}

/**
 * Clears the keys from the multiset.
 *
 * @param me the multi-set to clear
 */
void multiset_clear(multiset me)
{
    while (me->root) {
        multiset_remove_element(me, me->root);
    }
    me->size = 0;
}

/**
 * Frees the multi-set memory. Performing further operations after calling this
 * function results in undefined behavior.
 *
 * @param me the multi-set to free from memory
 *
 * @return NULL
 */
multiset multiset_destroy(multiset me)
{
    multiset_clear(me);
    free(me);
    return NULL;
}
