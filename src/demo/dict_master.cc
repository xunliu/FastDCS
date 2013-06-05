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
#include <sstream>
#include <iostream>

#include "src/base/logging.h"
#include "src/data/mysql_accessor.h"
#include "src/server/protofile.h"
#include "src/server/master.h"
#include "src/utils/string_printf.h"
#include "src/utils/string_codec.h"
#include "src/utils/string_util.h"
#include "src/utils/simple_hash.h"

using namespace std;
using namespace fastdcs;
using namespace fastdcs::server;

class DictMaster : public Master {
public:
  /*virtual*/ 
  void InitialTracker(struct settings_s settings) {
  	LOG(INFO) << "DictMaster::InitialTracker()";
  	mysql_connect.Connect(settings.mysql_database, 
  												settings.mysql_host, 
  												settings.mysql_user, 
  												settings.mysql_passwd, 
                          settings.mysql_port);

		Master::InitialTracker(settings);
	};
  
  /*virtual*/ 
  void FinalizeTracker() {
  	LOG(INFO) << "DictMaster::FinalizeTracker()";
		Master::FinalizeTracker();
	};

  /*virtual*/ 
  bool ExportTaskUDF(vector<FdcsTask> tasks) {
  	LOG(INFO) << "DictMaster::ExportTaskUDF(), tasks.size = " << tasks.size();
  	for (int i = 0; i < tasks.size(); ++i) {
  		LOG(INFO) << "task_id = " << tasks[i].task_id();

  		MysqlQuery query(&mysql_connect);
		  std::vector<KeyValuePair> key_values;
		  for (int ii = 0; ii < tasks[i].key_values_pairs_size(); ++ii) {
		    KeyValuesPair key_values_pair = tasks[i].key_values_pairs(ii);

		    ReadRecord(&key_values_pair, &key_values);
		    for (int iii = 0; iii < key_values.size(); ++iii) {
		    	string word = key_values[iii].key();
					word = StringReplace(word, "\"", "\\\"");
					word = StringReplace(word, "'", "\\\'");
		    	int count = KeyToInt32(key_values[iii].value());
		    	uint32 word_id = BKDRHash(word);

		    	string select_sql;
		    	SStringPrintf(&select_sql, "SELECT count FROM dict_word WHERE word_id = %lu;", word_id);		    	
		    	MysqlResult *sql_result = NULL;
				  if (!query.Execute(select_sql.data())) {
				    LOG(ERROR) << "SELECT count FROM dict_word faildure!";
				    continue;
				  }
				  sql_result = query.Store();
//				  LOG(INFO) << "sql_result->RowsSize() =  " << sql_result->RowsSize();
				  int old_count = 0;
				  if (sql_result->RowsSize() > 0) {
				    MysqlRow row = sql_result->FetchRow();
				    old_count = (int)row[0];
				  }
				  query.FreeResult(sql_result);

				  string insert_sql;
				  if (0 == old_count) {
						insert_sql = StringPrintf("INSERT INTO dict_word \
			    		(word_id, word, count, create_time) VALUES (%lu, '%s', %d, now())", 
			    		word_id, word.data(), count);
				  } else {
						insert_sql = StringPrintf("UPDATE dict_word SET count = %d, \
							update_time = now() WHERE word_id = %lu", 
			    		old_count + count, word_id);
				  }
				  if ((old_count + count) < 0 || old_count < 0 || count < 0) {
				    LOG(INFO) << "old_count = " << old_count;
				    LOG(INFO) << "count = " << count;
				    LOG(INFO) << "key_values[iii] = " << key_values[iii].value();
					  LOG(INFO) << "insert_sql = " << insert_sql;
					  continue;
				  }

				  if (!query.TryExecute(insert_sql.data())) {
				  	LOG(ERROR) << "INSERT dict_word[" << word_id << ", word = " << word << "] faildure!";
				  	continue;
				  }
		    }
		  }
  	}
		return true;
	};

  /*virtual*/ 
  bool ImportTaskUDF(vector<FdcsTask> &tasks) {
  	LOG(INFO) << "DictMaster::ImportTaskUDF()";

  	std::string select_sql, update_sql, update_where;
  	update_sql = "UPDATE dict_task SET task_status = 2 WHERE ";

  	std::string format = "SELECT task_id FROM dict_task WHERE task_status = 1 LIMIT %d;";
  	SStringPrintf(&select_sql, format.data(), fdcs_env_.PreloadTasks());
  	LOG(INFO) << select_sql;

	  MysqlQuery query(&mysql_connect);
	  if (!query.Execute(select_sql.data())) {
	    LOG(ERROR) << "select task from mysql failduer!";
	    return false;
	  }

	  MysqlResult *sql_result = query.Store();
	  int rows_size = sql_result->RowsSize();
	  LOG(INFO) << "sql_result->RowsSize() = " << sql_result->RowsSize();
	  if (0 == rows_size) return false;

	  FdcsTask task;
	  for (int i = 0; i < rows_size; ++i) {
	    MysqlRow row = sql_result->FetchRow();

	    std::string task_id = (char*)row[0];
	    LOG(INFO) << "task_id = " << task_id;
  		task.set_task_id(task_id);
  		tasks.push_back(task);

  		if (update_where.empty()) {
  			update_where = "task_id = '" + task_id + "'";
  		} else {
  			update_where = update_where + " or task_id = '" + task_id + "'";
  		}
	  }
	  query.FreeResult(sql_result);

	  update_sql = update_sql + update_where + ";";
//	  LOG(INFO) << update_sql;
	  if (!query.TryExecute(update_sql.data())) {
	  	LOG(ERROR) << "update task status faildure!";
	  	return false;
	  }

		return true;
  };
  
private:
	MysqlConnection mysql_connect;
};
REGISTER_FASTDCS_TRACKER(DictMaster);
