/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// Provide common define
//
#ifndef _BASE_COMMON_DEFINE_H
#define _BASE_COMMON_DEFINE_H

#include <limits.h>
#include <memory.h>

#include "src/base/common.h"

// FastDCS system define
#define FASTDCS_PACKAGE_NAME	"FastDCS"

static const int kMajorVersion = 0;
static const int kMinorVersion = 1;
static const int kRevisionNumber = 1;

// FastDCS tracker macro define
#define RESPONSE_RESULT_SUCCESS   "success"
#define RESPONSE_RESULT_FAILDURE  "faildure"
#define RESPONSE_RESULT_COMPLETE  "complete"

#define KEYVALUEPAIRS_TASK          "FdcsTask"
#define KEYVALUEPAIRS_TASK_COMPLETE "FdcsTaskComplete"
#define KEYVALUEPAIRS_FDCS_TIME     "FdcsTime"

// config file key define
// run as a daemon, true or false
#define RUN_AS_DAEMON "run_by_daemon"

// maximize core file limit
#define MAX_CORE_FILE "max_core_file"

// log file path, not set (empty) means no record log
#define INFO_LOG	"info_log"
#define WARN_LOG	"warn_log"
#define ERR_LOG		"err_log"

// log file max size
// default value is 10M
#define LOG_MAX_SIZE "log_max_size"

// linux username to run this program,
// not set (empty) means run by current user
#define RUN_BY_USER	"run_by_user"

// save PID file
#define PID_FILE	  "pid_file" 

// max concurrent connections the server supported
#define MAX_CONNECTIONS "max_connections"

// the socket buffer size to recv / send data
// this parameter must more than 8KB
// default value is 64KB
#define SOCKET_BUFF_SIZE "socket_buff_size"

// computing thread number
#define COMPUTING_THREADS "computing_threads" 

// preload task number
#define PRELOAD_TASKS "preload_tasks"

// task duplicate number
#define TASK_DUPLICATE "task_duplicate"

#define CLASS_FACTORY "class_factory"

// tracker_server can ocur more than once, and tracker_server format is
#define TRACKER_GROUP "tracker_group"

#define TRACKER_SERVER "tracker_server"

// connect timeout in seconds
#define CONNECT_TIMEOUT "connect_timeout"

// network timeout in seconds
#define NETWORK_TIMEOUT "network_timeout"

// heart beat interval in seconds
#define HEART_BEAT_INTERVAL "heart_beat_interval"

// disk usage report interval in seconds
#define STAT_REPORT_INTERVAL "stat_report_interval"

// lease interval in seconds
#define LEASE_TIMEOUT "lease_timeout"

// lease interval in seconds
#define MYSQL_DATABASE  "mysql_database"
#define MYSQL_HOST      "mysql_host"
#define MYSQL_USER      "mysql_user"
#define MYSQL_PASSWD    "mysql_passwd"
#define MYSQL_PORT_NO   "mysql_port"

// FastDCS system settings
typedef struct settings_s {
  char config_file[PATH_MAX];
  bool run_as_daemon;
  bool max_core_file;
  const char *info_log;
  const char *debug_log;
  const char *warn_log;
  const char *err_log;
  const char *run_by_user;
  const char *pid_file;
  const char *class_factory;
  const char *tracker_group;
  const char *tracker_server;
  const char *mysql_database;
  const char *mysql_host;
  const char *mysql_user;
  const char *mysql_passwd;
  int max_connections;
  int64 socket_buff_size;
  int64 log_max_size;
  int computing_threads;
  int preload_tasks;
  int task_duplicate;
  int connect_timeout;
  int network_timeout;
  int heart_beat_interval;
  int stat_report_interval;
  int lease_timeout;
  int mysql_port;
  settings_s() {
    memset(config_file, NULL, PATH_MAX * sizeof(char));
    run_as_daemon = false;
    max_core_file = false;
    info_log = NULL;
    debug_log = NULL;
    warn_log = NULL;
    err_log = NULL;
    log_max_size = 10 * 1024 * 1024; // default value is 10MB
    run_by_user = NULL;
    pid_file = NULL;
    max_connections = 256;
    socket_buff_size = 64 * 1024; // default value is 64KB
    computing_threads = 1;
    preload_tasks = 100;
    task_duplicate = 10;
    class_factory = NULL;
    tracker_group = NULL;
    tracker_server = NULL;
    mysql_database = NULL;
    mysql_host = NULL;
    mysql_user = NULL;
    mysql_passwd = NULL;
    connect_timeout = 30;
    network_timeout = 30;
    heart_beat_interval = 30;
    stat_report_interval = 30;
    lease_timeout = 10;
    mysql_port = 3306;
  }
} settings_t;

#endif  // _BASE_COMMON_DEFINE_H
