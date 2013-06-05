//
// This file exports the MD5 hashing algorithm.
//
#ifndef UTILS_MD5_HASH_H_
#define UTILS_MD5_HASH_H_

#include <string>

#include "src/base/common.h"

uint64 MD5Hash(const unsigned char *s, const unsigned int len);
uint64 MD5Hash(const std::string& s);

#endif  // UTILS_MD5_HASH_H_
