/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// Provide interface reads the configuration file.
//
#ifndef FASTDCS_BASE_CONFIG_FILE_H_
#define FASTDCS_BASE_CONFIG_FILE_H_

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "config_bst.h"


// A key/value pair found in a configuration file
struct config_pair_t {
  char *key;
  char *value;
};

// Data structure encapsulating a parsed configuration file 
// In the file:
//  - comments begin with \c comment_char and continue to a newline
//  - Keys are separated from values by \c sep_char
//  - String literals enclosed in \c str_char may contain any character,
// including spaces.
struct config_t {
  char comment_char; // Character that indicates the start of a comment
  char sep_char; // Character that separates keys from values
  char str_char; // Character that delineates string literals
  // ========== Implementation details below, subject to change ==========
  struct bst_table *table; // The parsed file data, stored as a binary tree
  struct bst_traverser traverser; // The table traverser
  int iterating; // Currently traversing?
};
typedef struct config_t config_t;

// Parse a configuration file
// This function will attempt to parse the configuration file
// \c path, using the comment, separator, and quote characters
// specified in \c data.
// This function allocates memory; use config_release to clean up.
// \param data The config_t in which to store the parsed data
// \param path The file to parse
// \return 0 if successful, nonzero otherwise
int config_parse(struct config_t *data, const char *path);

// Release memory associated with a configuration file
// This function frees any dynamically-allocated memory in \c data.
// \param data The config_t that is no longer needed
void config_release(struct config_t *data);

// Extract a value from a configuration file
// This function searches the parsed configuration file data for \c key,
// and returns the value associated with that key.<br>
// If \c key was found in the configuration file, the returned value 
// will never be 0.<br>
// The value returned belongs to ccl, and should not be free'd.
// \param data The config_t to query
// \param key The key to query
// \return The value associated with \c key, or 0 if \c key was not found
const char* config_get(const struct config_t *data, const char *key);

// Iterate through all key/value pairs in a configuration file
// This function allows iteration through all key/value pairs in a
// configuration file.<br>
// This function maintains internal state; to reset the iterator call
// config_reset.<br>
// The value returned belongs to ccl, and should not be free'd.<br>
// \param data The config_t to query
// \return A key/value pair, or 0 if no more exist in \c data
const struct config_pair_t* config_iterate(struct config_t *data);

// Reset a configuration file iterator
// This function resets the internal iterator in \c data to the first
// key/value pair.
// \param data The config_t to reset
void config_reset(struct config_t *data);


#endif  // FASTDCS_BASE_CONFIG_FILE_H_
