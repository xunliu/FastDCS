//
// Implementation the codec of varint32 using code from Google Protobuf.
//
#ifndef FASTDCS_BASE_VARINT32_H_
#define FASTDCS_BASE_VARINT32_H_

#include "src/base/common.h"

bool ReadVarint32(FILE* input, uint32* value);
bool WriteVarint32(FILE* output, uint32 value);

#endif  // FASTDCS_BASE_VARINT32_H_
