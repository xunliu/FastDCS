/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <map>
#include <iostream>

#include "src/base/logging.h"
#include "src/data/mysql_accessor.h"
#include "src/server/protofile.h"
#include "src/server/worker.h"
#include "src/server/tracker_protocol.pb.h"
#include "src/utils/string_printf.h"
#include "src/utils/string_codec.h"
#include "src/utils/split_string.h"

using namespace std;
using namespace fastdcs;
using namespace fastdcs::server;

class DictWorker : public Worker {
 public:
  /*virtual*/ 
  void InitialTracker(struct settings_s settings) {
  	LOG(INFO) << "DictWorker::InitialTracker()";
    mysql_connect_.Connect(settings.mysql_database, 
                           settings.mysql_host, 
                           settings.mysql_user, 
                           settings.mysql_passwd, 
                           settings.mysql_port);

		Worker::InitialTracker(settings);
	}
  
  /*virtual*/
  void FinalizeTracker() {
  	LOG(INFO) << "DictWorker::FinalizeTracker()";
		Worker::FinalizeTracker();
	}

  /*virtual*/ 
  bool ComputingUDF(FdcsTask &task) {
    LOG(INFO) << "DictWorker::ComputingUDF()";

    std::string select_sql, text;
    SStringPrintf(&select_sql, "SELECT text FROM dict_task WHERE task_id = %s;", 
                  task.task_id().data());
    LOG(INFO) << select_sql;

    MysqlQuery query(&mysql_connect_);
    if (!query.Execute(select_sql.data())) {
      LOG(ERROR) << "select text from mysql failduer!";
      return false;
    }

    MysqlResult *sql_result = query.Store();
    LOG(INFO) << "sql_result->RowsSize() = " << sql_result->RowsSize();
    if (sql_result->RowsSize() > 0) {
      MysqlRow row = sql_result->FetchRow();

      text.append((char*)row[0]);
//      LOG(INFO) << "text = " << text;
    }
    query.FreeResult(sql_result);

    // split string
    std::map<string /*work*/, int /*count*/> word_count_;
    std::vector<std::string> words;
    SplitStringUsing(text, " ", &words);
    for (int i = 0; i < words.size(); ++i) {
      if (true == words[i].empty()) continue;

      if (word_count_.end() == word_count_.find(words[i])) {
        word_count_[words[i]] = 1;
      } else {
        word_count_[words[i]] = word_count_[words[i]] + 1;        
      }
    }

    KeyValuesPair *key_values_pair = task.add_key_values_pairs();
    for (WordCountIter it = word_count_.begin(); it != word_count_.end(); ++it) {
      KeyValuePair key_value;
      key_value.set_key(it->first);
      string count = Int32ToKey(it->second);
      if (true == count.empty() || it->second < 0 || KeyToInt32(count) < 0) {
        LOG(INFO) << "it" << it->first << ", " << it->second << ", " << KeyToInt32(count);
        abort();
      }
      key_value.set_value(count);
      WriteRecord(key_values_pair, key_value);
    }

//    sleep(5); // test
    return true;
  }
 private:
  MysqlConnection mysql_connect_;
  
  typedef std::map<string, int>::iterator WordCountIter;
};
REGISTER_FASTDCS_TRACKER(DictWorker);
