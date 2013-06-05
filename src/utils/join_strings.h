/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#ifndef UTILS_JOIN_STRINGS_H_
#define UTILS_JOIN_STRINGS_H_

#include <string>

template <class ConstForwardIterator>
void JoinStrings(const ConstForwardIterator& begin,
                 const ConstForwardIterator& end,
                 const std::string& delimiter,
                 std::string* output) {
  output->clear();
  for (ConstForwardIterator iter = begin; iter != end; ++iter) {
    if (iter != begin) {
      output->append(delimiter);
    }
    output->append(*iter);
  }
}

template <class ConstForwardIterator>
std::string JoinStrings(const ConstForwardIterator& begin,
                        const ConstForwardIterator& end,
                        const std::string& delimiter) {
  std::string output;
  JoinStrings(begin, end, delimiter, &output);
  return output;
}

template <class Container>
std::string JoinStrings(const Container& container,
                        const std::string& delimiter = " ") {
  return JoinStrings(container.begin(), container.end(), delimiter);
}

#endif  // UTILS_JOIN_STRINGS_H_
