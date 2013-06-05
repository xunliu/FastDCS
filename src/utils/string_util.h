/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#ifndef UTILS_STRING_UTIL_H_
#define UTILS_STRING_UTIL_H_

#include <string>

std::string StringReplace(const std::string& input,					// The original string
                          const std::string& find,        	// To replace the original string
                          const std::string& replaceWith);	// Replacement purposes

int ParseBytes(const char *str_bytes, const int default_value, int64 *bytes);

#endif  // UTILS_STRING_UTIL_H_
