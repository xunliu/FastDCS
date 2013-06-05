/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#ifndef FASTDCS_SERVER_PROTOFILE_H_
#define FASTDCS_SERVER_PROTOFILE_H_

#include <stdio.h>
#include <vector>
#include <string>

#include <google/protobuf/generated_message_util.h>

#include "src/server/tracker_protocol.pb.h"

namespace fastdcs {
namespace server {

using namespace fastdcs;

bool ReadRecord(KeyValuesPair* pairs, 
                std::vector<TrackerStatus>* values);
bool ReadRecord(KeyValuesPair* pairs, 
                std::vector<FdcsTask>* values);
bool ReadRecord(KeyValuesPair* pairs, 
                std::vector<KeyValuePair>* values);
bool ReadRecord(KeyValuesPair* pairs, 
                std::map<std::string /*key*/, std::string /*value*/>* key_values);

bool WriteRecord(KeyValuesPair* pairs,
                const TrackerStatus& value);
bool WriteRecord(KeyValuesPair* pairs,
                const FdcsTask& value);
bool WriteRecord(KeyValuesPair* pairs,
                const KeyValuePair& value);

}  // namespace server
}  // namespace fastdcs

#endif  // FASTDCS_SERVER_PROTOFILE_H_
