#ifndef FASTDCS_BASE_SKIPLIST_H_
#define FASTDCS_BASE_SKIPLIST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// A skiplist level
typedef int8_t skiplist_level_t;
// A comparison result
typedef intmax_t skiplist_cmp_t;
// The length of a skiplist
typedef uintmax_t skiplist_length_t;

// Can be used as a @skiplist_cmp_t result
enum { SKIPLIST_CMP_LT = -1 };
enum { SKIPLIST_CMP_EQ =  0 };
enum { SKIPLIST_CMP_GT =  1 };

// Callback functions
typedef skiplist_cmp_t (*skiplist_cmp_fn)(const void *list_value,
                                          const void *user_value);

typedef skiplist_cmp_t (*skiplist_search_fn)(const void *value,
                                             const void *search);

typedef void (*skiplist_destroy_fn)(void *value);

typedef bool (*skiplist_inspect_fn)(const void *value, 
                                    char buffer[],
                                    const size_t buffer_size);

// A skiplist node
typedef struct skiplist_node_t {
  void *value;
  skiplist_level_t level;

  // An array of @level + 1 node pointers
  struct skiplist_node_t **forward;
} skiplist_node_t;

// A skiplist
typedef struct {
  skiplist_cmp_fn cmp_fn;
  skiplist_search_fn search_fn;
  skiplist_destroy_fn destroy_fn;
  skiplist_inspect_fn inspect_fn;

  skiplist_length_t length;
  skiplist_level_t level;
  skiplist_node_t *header;
} skiplist_t;

// Initialize the random number environment used for generating skiplist levels.
void skiplist_global_init(void);

// Initialize @list.
bool skiplist_init(skiplist_t *list);

// Destroy @list and free any used memory.
void skiplist_destroy(skiplist_t *list);

// Set @list's compare function -- the function used to compare two values
// when using @skiplist_contains, @skiplist_insert and @skiplist_delete.
static inline void skiplist_set_cmp_fn(skiplist_t *list,
                                       skiplist_cmp_fn fn) {
  list->cmp_fn = fn;
}

// Set @list's search function -- the function used to compare node values
// to a search value when using @skiplist_search.
static inline void skiplist_set_search_fn(skiplist_t *list,
                                          skiplist_search_fn fn) {
  list->search_fn = fn;
}

// Set @list's value destroy function -- the function called when a node is
// deleted to destroy its value.
static inline void skiplist_set_destroy_fn(skiplist_t *list,
                                           skiplist_destroy_fn fn) {
  list->destroy_fn = fn;
}

// Set @list's inspect function -- the function called to pretty-print a
// node's value.
static inline void skiplist_set_inspect_fn(skiplist_t *list,
                                           skiplist_inspect_fn fn) {
  list->inspect_fn = fn;
}

// Insert @value into @list.
bool skiplist_insert(skiplist_t *list, void *value);

// Delete @value from @list.
bool skiplist_delete(skiplist_t *list, void *value);

// Check if @list contains a value that matches @value, as determined by @list's
// @cmp_fn.
bool skiplist_contains(const skiplist_t *list, const void *value);

// Search @list for a value matching @search, as determined by @list's
// @search_fn.
void *skiplist_search(skiplist_t *list, const void *search);

// Get the number of values in @list.
static inline skiplist_length_t skiplist_length(const skiplist_t *list) {
  return list->length;
}

// Pretty-print the nodes in @list to @stream. If @list has an @inspect_fn, it
// will be used on each node's value.
void skiplist_inspect(const skiplist_t *list, FILE *stream);

// A skiplist iterator
typedef struct {
  skiplist_t *list;
  skiplist_node_t *cur_node;
} skiplist_iter_t;

// Initialize @iter and associate it with @list.
void skiplist_iter_init(skiplist_iter_t *iter, skiplist_t *list);

// Reset @iter to the beginning of its list.
void skiplist_iter_reset(skiplist_iter_t *iter);

// Get the next value from @iter or NULL if there are no more.
void *skiplist_iter_next(skiplist_iter_t *iter);

#endif  // FASTDCS_BASE_SKIPLIST_H_
