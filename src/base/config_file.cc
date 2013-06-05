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
#include <stdlib.h> // malloc, free
#include <stdio.h>	// fopen, fread, fclose
#include <string.h>	// strcmp, strdup
#include <ctype.h>	// isspace
#include <errno.h>	// ENOMEM, EINVAL, ENOENT

#include "config_file.h"

#define CONFIG_BUFSIZE 1024
#define CONFIG_TOKSIZE 32

enum {
  CONFIG_PARSE_INITIAL,
  CONFIG_PARSE_COMMENT,
  CONFIG_PARSE_QUOTED,
  CONFIG_PARSE_UNQUOTED,
  CONFIG_HANDLE_NEWLINE,
  CONFIG_HANDLE_SEP
};

static int config_bst_comparison_func(const void *bst_a, const void *bst_b, void *bst_param) {
  const struct config_pair_t *a = (const struct config_pair_t*) bst_a;
  const struct config_pair_t *b = (const struct config_pair_t*) bst_b;

  return strcmp(a->key, b->key);
}

int config_parse(struct config_t *data, const char *path) {
  FILE 			*f = NULL;
  char 			*buf = NULL, *p = NULL;
  char 			*token = NULL, *t = NULL, *tok_limit = NULL;
  int 			result, state, got_tok, tok_cap;
  size_t 		count, line;
  struct config_pair_t 	*pair = NULL;


  // Validate arguments
  if(data == 0 || path == 0)
    return EINVAL;
  
  // init 
  data->comment_char = '#';
  data->sep_char = '=';
  data->str_char = '"';

  // Setup local variables
  result = 0;
  pair = 0;
  line = 1;

  // Setup data struct
  data->table = bst_create(config_bst_comparison_func, 0, 0);
  if(data->table == 0) {
    return ENOMEM;    
    goto cleanup;
  }
  bst_t_init(&data->traverser, data->table);
  data->iterating = 0;

  // Open file 
  f = fopen(path, "r");
  if(f == 0) {
    fprintf(stderr, ": Unable to open '%s'\n", path);
    return ENOENT;
  }

  // Initialize file and token buffers
  buf = (char*) malloc(sizeof(char) * CONFIG_BUFSIZE);
  token = (char*) malloc(sizeof(char) * CONFIG_TOKSIZE);
  if(buf == 0 || token == 0) {
    result = ENOMEM;
    goto cleanup;
  }

  // Parse file
  state = CONFIG_PARSE_INITIAL;
  got_tok = 0;
  tok_cap = CONFIG_TOKSIZE;
  tok_limit = token + tok_cap;
  
  do {
    // Read a chunk
    count = fread(buf, sizeof(char), CONFIG_BUFSIZE, f);

    // Parse the input - manually increment p since not all
    // transitions should automatically consume a character
    for(p = buf; p < buf + count; /* ++p */ ) {

      switch(state) {

			// Initial parsing state
      case CONFIG_PARSE_INITIAL:
				if(*p == data->comment_char) {
				  state = CONFIG_PARSE_COMMENT;
				  ++p;
				}
				else if(*p == data->str_char) {
				  t = token;
				  state = CONFIG_PARSE_QUOTED;
				  ++p;
				}
				else if(*p == '\n') {
				  state = CONFIG_HANDLE_NEWLINE;
				}
				else if(*p == data->sep_char) {
				  state = CONFIG_HANDLE_SEP;
				  ++p;
				}
				else if(isspace(*p)) {
				  ++p;
				}
				else {
				  t = token;

				  // Enlarge buffer, if needed
				  if(t > tok_limit) {
				    int count = t - token;
				    token = (char*) realloc(token, tok_cap * 2);
				    if(token == 0) {
				      result = ENOMEM;
				      goto cleanup;
				    }
				    tok_cap *= 2;
				    tok_limit = token + tok_cap;
				    t = token + count;
				  }

				  *t++ = *p++;

				  state = CONFIG_PARSE_UNQUOTED;
				}
				break;
			// Parse comments
      case CONFIG_PARSE_COMMENT:
				if(*p == '\n') {
				  state = CONFIG_HANDLE_NEWLINE;
				}
				else {
				  ++p;
				}
				break;

			// Parse quoted strings
      case CONFIG_PARSE_QUOTED:
				if(*p == data->str_char) {
				  got_tok = 1;
				  *t = '\0';
				  state = CONFIG_PARSE_INITIAL;
				  ++p;
				} else if(*p == '\n') {
				  fprintf(stderr, "Unterminated string (%s:%i)\n", path, line);
				  
				  state = CONFIG_HANDLE_NEWLINE;
				} else {
				  // Enlarge buffer, if needed
				  if(t > tok_limit) {
				    int count = t - token;
				    token = (char*) realloc(token, tok_cap * 2);
				    if(token == 0) {
				      result = ENOMEM;
				      goto cleanup;
				    }
				    tok_cap *= 2;
				    tok_limit = token + tok_cap;
				    t = token + count;
				  }

				  *t++ = *p++;
				}
				break;
	
			// Parse unquoted strings
      case CONFIG_PARSE_UNQUOTED:
				if(*p == data->comment_char) {
				  if(t != token) {
				    got_tok = 1;
				    *t = '\0';
				  }
				  state = CONFIG_PARSE_COMMENT;
				  ++p;
				} else if(*p == '\n') {
				  if(t != token) {
				    got_tok = 1;
				    *t = '\0';
				  }
				  state = CONFIG_HANDLE_NEWLINE;
				} else if(*p == data->sep_char) {
				  if(t != token) {
				    got_tok = 1;
				    *t = '\0';
				  }

				  state = CONFIG_HANDLE_SEP;
				  ++p;
				} else if(isspace(*p)) {
					// In this mode a space ends the current token
				  if(t != token) {
				    got_tok = 1;
				    *t = '\0';
				  }
				  
				  state = CONFIG_PARSE_INITIAL;
				  ++p;
				} else {
				  // Enlarge buffer, if needed
				  if(t > tok_limit) {
				    int count = t - token;
				    token = (char*) realloc(token, tok_cap * 2);
				    if(token == 0) {
				      result = ENOMEM;
				      goto cleanup;
				    }
				    tok_cap *= 2;
				    tok_limit = token + tok_cap;
				    t = token + count;
				  }

				  *t++ = *p++;
				}
				break;

			// Process separator characters
      case CONFIG_HANDLE_SEP:
				if(got_tok == 0) {
				  pair = 0;
				  fprintf(stderr, ": Missing key (%s:%i)\n", path, line);
				} else if(config_get(data, token) != 0) {
				  pair = 0;
				  fprintf(stderr, ": Ignoring duplicate key '%s' (%s:%i)\n", 
					  token, path, line);
				} else {
				  pair = (struct config_pair_t*) malloc(sizeof(struct config_pair_t));
				  if(pair == 0) {
				    result = ENOMEM;
				    goto cleanup;
				  }

				  pair->key = strdup(token);
				  pair->value = 0;
				  if(pair->key == 0) {
				    result = ENOMEM;
				    goto cleanup;
				  }
				}

				got_tok = 0;
				state = CONFIG_PARSE_INITIAL;
				break;

			// Process newlines
      case CONFIG_HANDLE_NEWLINE:
				if(pair != 0) {
				  // Some type of token was parsed
				  if(got_tok == 1) {
				    pair->value = strdup(token);
				  } else {
				    // In this case we read a key but no value
				    pair->value = strdup("");
				  }
				  
				  if(pair->value == 0) {
				    result = ENOMEM;
				    goto cleanup;
				  }

				  if(bst_probe(data->table, pair) == 0) {
				    result = ENOMEM;
				    goto cleanup;
				  }
				}

				got_tok = 0;
				pair = 0;
				state = CONFIG_PARSE_INITIAL;
				++line;
				++p;
				break;
      }
    }    
  } while(feof(f) == 0);

 cleanup:
  if(pair != 0) {
    free(pair->key);
    free(pair->value);
    free(pair);
  }
  free(buf);
  free(token);
  fclose(f);

  return result;
}

const char* config_get(const struct config_t *data, const char *key) {
  const struct config_pair_t 	*pair;
  struct config_pair_t 		temp;

  if(data == 0 || key == 0)
    return 0;

  temp.key = (char*) key;
  temp.value = 0;

  pair = (const struct config_pair_t*) bst_find(data->table, &temp);
  
  return pair == 0 ? 0 : pair->value;
}

const struct config_pair_t* config_iterate(struct config_t *data) {
  struct config_pair_t *pair;

  if(data == 0)
    return 0;

  if(data->iterating) {
    pair = (struct config_pair_t *)bst_t_next(&data->traverser);
  } else {
    data->iterating = 1;
    pair = (struct config_pair_t *)bst_t_first(&data->traverser, data->table);
  }

  return pair;
}

static void config_bst_item_func(void *bst_item, void *bst_param) {
  struct config_pair_t *pair = (struct config_pair_t*) bst_item;
  free(pair->key);
  free(pair->value);
  free(pair);
}

void config_release(struct config_t *data) {
  if(data == 0)
    return;

  bst_destroy(data->table, config_bst_item_func);
}

void config_reset(struct config_t *data) {
  if(data != 0) {
    data->iterating = 0;
  }
}
