#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "src/base/skip_list.h"

#define UINT16_MAX 0xffff

// A skiplist's maximum level
enum { SKIPLIST_LEVEL_MAX = 8 };
// A skiplist's minimum level
enum { SKIPLIST_LEVEL_MIN = 0 };
// The amount of possible levels
enum { SKIPLIST_LEVEL_COUNT = SKIPLIST_LEVEL_MAX - SKIPLIST_LEVEL_MIN + 1 };

// An array of nodes that need to be updated after an insert or delete operation
typedef skiplist_node_t *skiplist_update_t[SKIPLIST_LEVEL_COUNT];

void skiplist_global_init(void) {
  srand(time(NULL));
}

// Generate a random skiplist level.
static skiplist_level_t skiplist_level_generate(void) {
  // This constant is found by 1 / P, where P = 0.25.
  enum { P_INVERSE = 4 };

  // The original algorithm's random number is in the range [0, 1), so
  // max M = 1. Its ceiling C = M * P = 1 * P = P.
  //
  // Our random number is in the range [0, UINT16_MAX], so M = UINT16_MAX. Therefore,
  // C = UINT16_MAX * P = UINT16_MAX / P_INVERSE.
  enum { P_CEIL = UINT16_MAX / P_INVERSE };

  skiplist_level_t level = SKIPLIST_LEVEL_MIN;

  while ((uint16_t)(rand()) < P_CEIL)
    level += 1;

  if (level < SKIPLIST_LEVEL_MAX)
    return level;

  return SKIPLIST_LEVEL_MAX;
}

// Normalize @cmp to exactly @SKIPLIST_CMP_LT, @SKIPLIST_CMP_EQ, or
// @SKIPLIST_CMP_GT.
//
//   skiplist_cmp_normalize(-3) -> SKIPLIST_CMP_LT (-1)
//   skiplist_cmp_normalize(0)  -> SKIPLIST_CMP_EQ (0)
//   skiplist_cmp_normalize(50) -> SKIPLIST_CMP_GT (1)
//
static skiplist_cmp_t skiplist_cmp_normalize(const skiplist_cmp_t cmp) {
  if (cmp <= SKIPLIST_CMP_LT)
    return SKIPLIST_CMP_LT;

  if (cmp >= SKIPLIST_CMP_GT)
    return SKIPLIST_CMP_GT;

  return SKIPLIST_CMP_EQ;
}

// Get the node immediately after @node.
static inline skiplist_node_t *skiplist_node_next(const skiplist_node_t *node) {
  return node->forward[SKIPLIST_LEVEL_MIN];
}

// Create a new level @level node with value @value. The node should eventually
// be destroyed with @skiplist_node_destroy.
//
//   @return: a new node on success and NULL otherwise.
//
static skiplist_node_t *skiplist_node_new(const skiplist_level_t level,
                                          void *value)
{
  skiplist_node_t *new_node = (skiplist_node_t *)
    (malloc(sizeof(skiplist_node_t)));

  if (!new_node)
    return NULL;

  new_node->value = value;
  new_node->level = level;

  // A level 0 node still needs to hold 1 forward pointer, etc.
  new_node->forward = (skiplist_node_t **)
    (calloc(level + 1, sizeof(skiplist_node_t *)));

  if (new_node->forward)
    return new_node;

  free(new_node);

  return NULL;
}

// Create a new header node -- a node with the value NULL and level
// @SKIPLIST_LEVEL_MIN.
//
//   @return: see @skiplist_node_new.
//
static inline skiplist_node_t *skiplist_header_node_new(void) {
  return skiplist_node_new(SKIPLIST_LEVEL_MAX, NULL);
}

// Destroy @node. If @list has a @destroy_fn, use it to destroy @node's value.
static void skiplist_node_destroy(skiplist_node_t *node, skiplist_t *list) {
  if (list->destroy_fn && node != list->header)
    list->destroy_fn(node->value);

  free(node->forward);
  free(node);
}

bool skiplist_init(skiplist_t *list) {
  list->cmp_fn = NULL;
  list->search_fn = NULL;
  list->destroy_fn = NULL;
  list->inspect_fn = NULL;

  list->length = 0;
  list->level = SKIPLIST_LEVEL_MIN;
  list->header = skiplist_header_node_new();

  return list->header != NULL;
}

void skiplist_destroy(skiplist_t *list) {
  skiplist_node_t *cur_node = list->header;
  skiplist_node_t *fwd_node;

  do {
    fwd_node = skiplist_node_next(cur_node);
    skiplist_node_destroy(cur_node, list);
  } while ((cur_node = fwd_node));
}

// An operation to perform after comparing a user value or search with a node's
// value
typedef enum {
  OP_GOTO_NEXT_LEVEL,
  OP_GOTO_NEXT_NODE,
  OP_FINISH
} op_t;

static op_t op_insert(const skiplist_t *list,
                      const skiplist_node_t *fwd_node,
                      const void *value)
{
  if (!fwd_node)
    return OP_GOTO_NEXT_LEVEL;

  switch (skiplist_cmp_normalize(list->cmp_fn(fwd_node->value, value))) {
    case SKIPLIST_CMP_LT: return OP_GOTO_NEXT_NODE;
    case SKIPLIST_CMP_EQ: return OP_FINISH;
  }

  return OP_GOTO_NEXT_LEVEL;
}

static op_t op_delete(const skiplist_t *list,
                      const skiplist_node_t *fwd_node,
                      const void *value)
{
  if (!fwd_node)
    return OP_GOTO_NEXT_LEVEL;

  switch (skiplist_cmp_normalize(list->cmp_fn(fwd_node->value, value))) {
    case SKIPLIST_CMP_LT: return OP_GOTO_NEXT_NODE;
  }

  return OP_GOTO_NEXT_LEVEL;
}

static op_t op_contains(const skiplist_t *list,
                        const skiplist_node_t *fwd_node,
                        const void *value)
{
  if (!fwd_node)
    return OP_GOTO_NEXT_LEVEL;

  switch (skiplist_cmp_normalize(list->cmp_fn(fwd_node->value, value))) {
    case SKIPLIST_CMP_LT: return OP_GOTO_NEXT_NODE;
    case SKIPLIST_CMP_EQ: return OP_FINISH;
  }

  return OP_GOTO_NEXT_LEVEL;
}

static op_t op_search(const skiplist_t *list,
                      const skiplist_node_t *fwd_node,
                      const void *search)
{
  if (!fwd_node)
    return OP_GOTO_NEXT_LEVEL;

  switch (skiplist_cmp_normalize(list->search_fn(fwd_node->value, search))) {
    case SKIPLIST_CMP_LT: return OP_GOTO_NEXT_NODE;
    case SKIPLIST_CMP_EQ: return OP_FINISH;
  }

  return OP_GOTO_NEXT_LEVEL;
}

bool skiplist_insert(skiplist_t *list, void *value) {
  skiplist_update_t update;
  skiplist_level_t update_level;

  skiplist_node_t *cur_node = list->header;
  skiplist_level_t level = list->level;

  while ((update_level = level) >= SKIPLIST_LEVEL_MIN) {
    skiplist_node_t *fwd_node = cur_node->forward[level];

    switch (op_insert(list, fwd_node, value)) {
      case OP_FINISH:
        // Reinsert @value to ensure correct sorting.
        return skiplist_delete(list, fwd_node->value) &&
               skiplist_insert(list, value);

      case OP_GOTO_NEXT_NODE:  cur_node = fwd_node; break;
      case OP_GOTO_NEXT_LEVEL: level -= 1;
    }

    update[update_level] = cur_node;
  }

  const skiplist_level_t new_node_level = skiplist_level_generate();

  if (new_node_level > list->level) {
    for (level = list->level + 1; level <= new_node_level; level += 1)
      update[level] = list->header;

    list->level = new_node_level;
  }

  skiplist_node_t *new_node = skiplist_node_new(new_node_level, value);

  if (!new_node)
    return false;

  // Drop @new_node into @list.
  for (level = SKIPLIST_LEVEL_MIN; level <= new_node_level; level += 1) {
    new_node->forward[level] = update[level]->forward[level];
    update[level]->forward[level] = new_node;
  }

  list->length += 1;

  return true;
}

bool skiplist_delete(skiplist_t *list, void *value) {
  skiplist_update_t update;
  skiplist_level_t update_level;

  skiplist_node_t *cur_node = list->header;
  skiplist_level_t level = list->level;

  while ((update_level = level) >= SKIPLIST_LEVEL_MIN) {
    skiplist_node_t *fwd_node = cur_node->forward[level];

    switch (op_delete(list, fwd_node, value)) {
      case OP_GOTO_NEXT_NODE:  
        cur_node = fwd_node; 
        break;
      case OP_GOTO_NEXT_LEVEL: 
        level -= 1;
      default:
        break;
    }

    update[update_level] = cur_node;
  }

  // The immediate forward node should be the matching node...
  skiplist_node_t *found_node = skiplist_node_next(cur_node);

  // ...unless we're at the end of the list or the value doesn't exist.
  if (!found_node || list->cmp_fn(found_node->value, value) >= SKIPLIST_CMP_GT)
    return false;

  // Splice @found_node out of @list.
  for (level = SKIPLIST_LEVEL_MIN; level <= list->level; level += 1)
    if (update[level]->forward[level] == found_node)
      update[level]->forward[level] = found_node->forward[level];

  skiplist_node_destroy(found_node, list);

  // Remove unused levels from @list -- stop removing levels as soon as a used
  // level is found. Unused levels can occur if @found_node had the highest
  // level.
  for (level = list->level; level >= SKIPLIST_LEVEL_MIN; level -= 1) {
    if (list->header->forward[level])
      break;

    list->level -= 1;
  }

  list->length -= 1;

  return true;
}

bool skiplist_contains(const skiplist_t *list, const void *value) {
  const skiplist_node_t *cur_node = list->header;
  skiplist_level_t level = list->level;

  while (level >= SKIPLIST_LEVEL_MIN) {
    const skiplist_node_t *fwd_node = cur_node->forward[level];

    switch (op_contains(list, fwd_node, value)) {
      case OP_FINISH:          return true;
      case OP_GOTO_NEXT_NODE:  cur_node = fwd_node; break;
      case OP_GOTO_NEXT_LEVEL: level -= 1;
    }
  }

  return false;
}

void *skiplist_search(skiplist_t *list, const void *search) {
  skiplist_node_t *cur_node = list->header;
  skiplist_level_t level = list->level;

  while (level >= SKIPLIST_LEVEL_MIN) {
    skiplist_node_t *fwd_node = cur_node->forward[level];

    switch (op_search(list, fwd_node, search)) {
      case OP_FINISH:          return fwd_node->value;
      case OP_GOTO_NEXT_NODE:  cur_node = fwd_node; break;
      case OP_GOTO_NEXT_LEVEL: level -= 1;
    }
  }

  return NULL;
}

void skiplist_inspect(const skiplist_t *list, FILE *stream) {
  enum { BUFFER_SIZE = 256 };
  typedef char buffer_t[BUFFER_SIZE];

  const skiplist_node_t *cur_node = list->header;

  do {
    if (cur_node == list->header)
      fprintf(stream, "Header ");
    else
      fprintf(stream, "Node ");

    fprintf(stream, "%p {level: %d", (void *)(cur_node), cur_node->level);

    if (list->inspect_fn && cur_node != list->header) {
      buffer_t buffer;

      if (list->inspect_fn(cur_node->value, buffer, BUFFER_SIZE))
        fprintf(stream, ", value: %s", buffer);
    }

    fprintf(stream, "}\n");

    skiplist_level_t level;

    for (level = cur_node->level; level >= SKIPLIST_LEVEL_MIN; level -= 1)
      fprintf(stream, "  |%d| -> %p\n", level,
        (void *)(cur_node->forward[level]));
  } while ((cur_node = skiplist_node_next(cur_node)));
}

void skiplist_iter_init(skiplist_iter_t *iter, skiplist_t *list) {
  iter->list = list;
  skiplist_iter_reset(iter);
}

void skiplist_iter_reset(skiplist_iter_t *iter) {
  iter->cur_node = iter->list->header;
}

void *skiplist_iter_next(skiplist_iter_t *iter) {
  iter->cur_node = skiplist_node_next(iter->cur_node);

  if (iter->cur_node)
    return iter->cur_node->value;

  return NULL;
}