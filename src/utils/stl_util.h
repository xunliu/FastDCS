/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// This file contains facilities that enhance the STL.
//
#ifndef UTILS__STL_UTIL_H_
#define UTILS__STL_UTIL_H_

#include "stddef.h"

// Delete elements (in pointer type) in a STL container like vector,
// list, and deque.
template <class Container>
void STLDeleteElementsAndClear(Container* c) {
  for (typename Container::iterator iter = c->begin();
       iter != c->end(); ++iter) {
    if (*iter != NULL) {
      delete *iter;
    }
  }
  c->clear();
}

// Delete elements (in pointer type) in a STL associative container
// like map and hash_map.
template <class AssocContainer>
void STLDeleteValuesAndClear(AssocContainer* c) {
  for (typename AssocContainer::iterator iter = c->begin();
       iter != c->end(); ++iter) {
    if (iter->second != NULL) {
      delete iter->second;
    }
  }
  c->clear();
}

// Free elements (in pointer type) in a STL associative container
// like map and hash_map.
template <class AssocContainer>
void STLFreeValuesAndClear(AssocContainer* c) {
  for (typename AssocContainer::iterator iter = c->begin();
       iter != c->end(); ++iter) {
    if (iter->second != NULL) {
      free(iter->second);
    }
  }
  c->clear();
}

#endif  // UTILS__STL_UTIL_H_
