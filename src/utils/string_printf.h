//
// This code comes from the re2 project host on Google Code
// (http://code.google.com/p/re2/), in particular, the following source file
// http://code.google.com/p/re2/source/browse/util/stringprintf.cc
//
#ifndef UTILS_STRINGPRINTF_H_
#define UTILS_STRINGPRINTF_H_

#include <string>
#include <string.h>

// va_copy is c99 - anything before that, and its upto the compiler... as pointed out, 
// gcc 2.95 dosnt have it,depends on your definition of absolutely ancient, 
// but the gcc 2.95.4 that comes with freebsd 4.x does not support va_copy 
#ifndef va_copy 
# ifdef __va_copy 
# define va_copy(DEST,SRC) __va_copy((DEST),(SRC)) 
# else 
# define va_copy(DEST, SRC) memcpy((&DEST), (&SRC), sizeof(va_list)) 
# endif 
#endif 

std::string StringPrintf(const char* format, ...);
void SStringPrintf(std::string* dst, const char* format, ...);
void StringAppendF(std::string* dst, const char* format, ...);

#endif  // UTILS_STRINGPRINTF_H_
