/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <unistd.h>
#include <string>

#include "src/base/common.h"
#include "src/data/mysql_accessor.h"
#include "src/utils/string_codec.h"
#include "src/utils/string_util.h"

using namespace fastdcs;

void CreateDictTable();
void CreateDictData();
void TruncateDictData();
void DropDictTable();

MysqlConnection connection;

int main(int argc, char const *argv[]) {
  bool conn = connection.Connect("FastDCS", "localhost", "FastDCS", "fastdcs", 3306);
  if (!conn)
  	LOG(ERROR) << "connect mysql faildure!";

  char cmd[20];
  printf("----- 0:quit！-----\n");
  printf("----- 1:create dict table -----\n");
  printf("----- 2:create dict data -----\n");
  printf("----- 3:truncate dict data -----\n");
  printf("----- 4:drop dict table -----\n");

  while (strncmp("0", cmd, 1) != 0) {
    fgets(cmd, 20, stdin);
    if (strncmp("1", cmd, 1) == 0) {
      CreateDictTable();
    } else if (strncmp("2", cmd, 1) == 0) {
      CreateDictData();
    } else if (strncmp("3", cmd, 1) == 0) {
      TruncateDictData();
    } else if (strncmp("4", cmd, 1) == 0) {
      DropDictTable();
    }
  }

	return 0;
}

void CreateDictTable() {
  MysqlQuery query(&connection);
  if (!query.Execute("SELECT * FROM dict_task;")) {
    query.Execute("CREATE TABLE dict_task ( 									\
										task_id VARCHAR(32) NOT NULL PRIMARY KEY,	\
										task_status INTEGER default 0, 						\
										text VARCHAR(20480),											\
                    create_time DATETIME,                     \
                    update_time DATETIME                      \
									)ENGINE=InnoDB DEFAULT CHARSET=utf8;");
  }
  LOG(INFO) << "CREATE TABLE dict_task";

  if (!query.Execute("SELECT * FROM dict_word;")) {
    query.Execute("CREATE TABLE dict_word (                         \
                    word_id INTEGER UNSIGNED NOT NULL PRIMARY KEY,  \
										word VARCHAR(256) NOT NULL,                     \
										count INTEGER default 0, 								        \
                    create_time DATETIME,                           \
                    update_time DATETIME                            \
									)ENGINE=InnoDB DEFAULT CHARSET=utf8;");
  }
  LOG(INFO) << "CREATE TABLE dict_word";
}

void DropDictTable() {
  MysqlQuery query(&connection);
  if (!query.TryExecute("DROP TABLE dict_task;")) {
    LOG(ERROR) << "DROP TABLE dict_task faildure!";
  } else {
    LOG(INFO) << "DROP TABLE dict_task success!";
  }

  if (!query.TryExecute("DROP TABLE dict_word;")) {
    LOG(ERROR) << "DROP TABLE dict_word faildure!";
  } else {
    LOG(INFO) << "DROP TABLE dict_word success!";
  }
}

void CreateDictData() {
	float time_use = 0;
	struct timeval start, end;
	
	gettimeofday(&start, NULL);
	LOG(INFO) << "start.tv_sec:" << start.tv_sec;
	LOG(INFO) << "start.tv_usec:" << start.tv_usec;

	FILE* file = NULL;
	char* line = NULL;
	size_t len = 0;
	size_t read;

	file = fopen("dict.txt", "r");
	CHECK(file);

	int32_t count = 1;
	std::string sql, line_text;

	MysqlQuery query(&connection);
	while ((read = getline(&line, &len, file)) != -1) {
		line_text = line;
		line_text = StringReplace(line_text, "\"", "\\\"");
		line_text = StringReplace(line_text, "'", "\\\'");

		sql = "INSERT INTO dict_task(task_id, task_status, text, create_time) VALUES ('";
		sql += Int32ToKey(count);
		sql += "' , 1, '";
		sql += line_text;
		sql += "', now())";

	  if (!query.TryExecute(sql.data())) {
	  	LOG(ERROR) << "INSERT INTO dict_task[" << count << "] faildure!";
//	  	LOG(ERROR) << sql;
	  	fclose(file);
	  	return;
	  }
		count ++;
	}
	LOG(INFO) << "file line count = " << count;
	if (line)
		free(line);

	fclose(file);

  gettimeofday(&end, NULL);
  LOG(INFO) << "end.tv_sec:" << end.tv_sec;
  LOG(INFO) << "end.tv_usec:" << end.tv_usec;
  time_use = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec); // ms
  LOG(INFO) << "time_use is " << time_use/1000000 << "s"; 
}

void TruncateDictData() {
  MysqlQuery query(&connection);
  if (!query.TryExecute("TRUNCATE TABLE dict_task;")) {
    LOG(ERROR) << "TRUNCATE TABLE dict_task faildure!";
  } else {
  	LOG(INFO) << "TRUNCATE TABLE dict_task success."; 	
  }

  if (!query.TryExecute("TRUNCATE TABLE dict_word;")) {
    LOG(ERROR) << "TRUNCATE TABLE dict_word faildure!";
  } else {
  	LOG(INFO) << "TRUNCATE TABLE dict_word success.";	
  }
}

// SELECT * FROM dict_word order by count desc limit 0, 10;
// SELECT count(1), MAX(count) FROM dict_word;
// SELECT count(1), task_status FROM dict_task GROUP BY task_status;
