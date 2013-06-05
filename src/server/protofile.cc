/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <string>

#include "src/base/common.h"
#include "src/server/protofile.h"

namespace fastdcs {
namespace server {

using std::string;

typedef ::google::protobuf::Message ProtoMessage;

//-----------------------------------------------------------------------------
// The following two specializations of function templates parse the read value
// into either (1) an std::string object or (2) a protocol message.
//-----------------------------------------------------------------------------
template <class ProtoMessage>
void ParseValue(string msg_bytes, ProtoMessage* msg) {
  msg->Clear();
  msg->ParseFromArray(msg_bytes.data(), msg_bytes.length());
}

//-----------------------------------------------------------------------------
// Given ParseValue, the following functiontemplate reads a record
//-----------------------------------------------------------------------------
template <class ValueType>
bool ReadRecord(KeyValuesPair* pairs, std::vector<ValueType>* values) {
  ValueType value;
  for (int index = 0; index < pairs->value_size(); ++index) {
    string value_bytes = pairs->value(index);
    ParseValue<ValueType>(value_bytes, &value);
    values->push_back(value);
  }
  return true;
}

//-----------------------------------------------------------------------------
// Interfaces defined in mrml_recordio.h are invocations of
// realizations of function template ReadRecord.
//-----------------------------------------------------------------------------
bool ReadRecord(KeyValuesPair* pairs, std::vector<TrackerStatus>* values) {
  return ReadRecord<TrackerStatus>(pairs, values);
}

bool ReadRecord(KeyValuesPair* pairs, std::vector<FdcsTask>* values) {
  return ReadRecord<FdcsTask>(pairs, values);
}

bool ReadRecord(KeyValuesPair* pairs, std::vector<KeyValuePair>* values) {
  return ReadRecord<KeyValuePair>(pairs, values);
}

bool ReadRecord(KeyValuesPair* pairs, 
                std::map<std::string, std::string>* key_values) {
  KeyValuePair value;
  for (int index = 0; index < pairs->value_size(); ++index) {
    string value_bytes = pairs->value(index);
    ParseValue<KeyValuePair>(value_bytes, &value);
    key_values->insert(std::pair<std::string, std::string>(value.key(), value.value()));
  }
  return true;
}
//-----------------------------------------------------------------------------
// The following specializations of function templates serialize the read
// value into either (1) an std::string object or (2) a protocol message.
//-----------------------------------------------------------------------------
template <class ProtoMessage>
void SerializeValue(KeyValuesPair* pairs, const ProtoMessage& msg) {
  int msg_size = msg.ByteSize();
  void *msg_buffer = malloc(msg_size);
  msg.SerializeToArray(msg_buffer, msg_size);
  pairs->add_value(msg_buffer, msg_size);
  free(msg_buffer);
}

//-----------------------------------------------------------------------------
// This function template saves a key-value pairs.
//-----------------------------------------------------------------------------
template <class ValueType>
bool WriteRecord(KeyValuesPair* pairs, const ValueType& value) {
  SerializeValue<ValueType>(pairs, value);
  return true;
}

//-----------------------------------------------------------------------------
// Interfaces defined in mrml_recordio.h are invocations of
// realizations of function template ReadRecord.
//-----------------------------------------------------------------------------
bool WriteRecord(KeyValuesPair* pairs, const TrackerStatus& value) {
  return WriteRecord<TrackerStatus>(pairs, value);
}

bool WriteRecord(KeyValuesPair* pairs, const FdcsTask& value) {
  return WriteRecord<FdcsTask>(pairs, value);
}

bool WriteRecord(KeyValuesPair* pairs, const KeyValuePair& value) {
  return WriteRecord<KeyValuePair>(pairs, value);
}

}  // namespace server
}  // namespace fastdcs
