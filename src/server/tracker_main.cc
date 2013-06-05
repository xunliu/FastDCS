/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <pwd.h>
#include <sysexits.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/resource.h> // for getrlimit
#include <sys/ioctl.h>

#include "src/base/config_file.h"
#include "src/base/common_define.h"
#include "src/server/epoller.h"
#include "src/server/tracker.h"
#include "src/system/daemon.h"
#include "src/utils/string_printf.h"
#include "src/utils/string_util.h"

using fastdcs::server::poll_event_t;
using fastdcs::server::poll_event_element_t;
using fastdcs::server::Epoller;
using fastdcs::server::Tracker;

struct config_t config_; // system config file object
struct settings_s settings_;
bool daemon_quit_ = false;

bool LoadConfig(char* config_file) {
  if (0 == strcmp(config_file, "")) {
    return false;
  }
  std::cout << "load config from \"" << config_file << "\"\n";

  const struct config_pair_t *iter = NULL;
  strcpy(settings_.config_file, config_file);
  CHECK_EQ(config_parse(&config_, config_file), 0);

  while((iter = config_iterate(&config_)) != 0) {
    std::cout << iter->key << "=[" << iter->value << "]\n";
  }

  std::string run_as_daemon = config_get(&config_, RUN_AS_DAEMON);
  settings_.run_as_daemon = (0 == run_as_daemon.compare("true")) ? true : false;
  std::string max_core_file = config_get(&config_, MAX_CORE_FILE);
  settings_.max_core_file = (0 == max_core_file.compare("true")) ? true : false;
  settings_.info_log = config_get(&config_, INFO_LOG);
  settings_.warn_log = config_get(&config_, WARN_LOG);
  settings_.err_log = config_get(&config_, ERR_LOG);
  settings_.run_by_user = config_get(&config_, RUN_BY_USER);
  settings_.pid_file = config_get(&config_, PID_FILE);
  settings_.class_factory = config_get(&config_, CLASS_FACTORY);
  settings_.tracker_group = config_get(&config_, TRACKER_GROUP);
  settings_.tracker_server = config_get(&config_, TRACKER_SERVER);
  settings_.mysql_database = config_get(&config_, MYSQL_DATABASE);
  settings_.mysql_host = config_get(&config_, MYSQL_HOST);
  settings_.mysql_user = config_get(&config_, MYSQL_USER);
  settings_.mysql_passwd = config_get(&config_, MYSQL_PASSWD);

  const char *log_max_size = config_get(&config_, LOG_MAX_SIZE);
  if (log_max_size) 
    ParseBytes(log_max_size, settings_.log_max_size, &settings_.log_max_size);
  else 
    std::cout << LOG_MAX_SIZE << " not config, default value " 
              << settings_.log_max_size << "\n";

  const char *socket_buff_size = config_get(&config_, SOCKET_BUFF_SIZE);
  if (socket_buff_size) 
    ParseBytes(socket_buff_size, settings_.socket_buff_size, &settings_.socket_buff_size);
  else 
    std::cout << SOCKET_BUFF_SIZE << " not config, default value " 
              << settings_.socket_buff_size << "\n";
  if (settings_.socket_buff_size < 8 * 1024 || settings_.socket_buff_size > 512 * 1024) {
    std::cout << ("settings_.socket_buff_size = ") << settings_.socket_buff_size << "\n";
    settings_.socket_buff_size = 64 * 1024;
  }

  const char *max_connections = config_get(&config_, MAX_CONNECTIONS);
  if (max_connections) 
    settings_.max_connections = atoi(max_connections);
  else 
    std::cout << MAX_CONNECTIONS << " not config, default value " 
              << settings_.max_connections << "\n";

  const char *task_duplicate = config_get(&config_, TASK_DUPLICATE);
  if (task_duplicate) 
    settings_.task_duplicate = atoi(task_duplicate);
  else 
    std::cout << PRELOAD_TASKS << " not config, default value " 
              << settings_.task_duplicate << "\n";
      
  const char *preload_tasks = config_get(&config_, PRELOAD_TASKS);
  if (preload_tasks) 
    settings_.preload_tasks = atoi(preload_tasks);
  else 
    std::cout << PRELOAD_TASKS << " not config, default value " 
              << settings_.preload_tasks << "\n";

  const char *computing_threads = config_get(&config_, COMPUTING_THREADS);
  if (computing_threads) 
    settings_.computing_threads = atoi(computing_threads);
  else 
    std::cout << COMPUTING_THREADS << " not config, default value " 
              << settings_.computing_threads << "\n";

  const char *connect_timeout = config_get(&config_, CONNECT_TIMEOUT);
  if (connect_timeout) 
    settings_.connect_timeout = atoi(connect_timeout);
  else 
    std::cout << CONNECT_TIMEOUT << " not config, default value " 
              << settings_.connect_timeout << "\n";

  const char *network_timeout = config_get(&config_, NETWORK_TIMEOUT);
  if (network_timeout) 
    settings_.network_timeout = atoi(network_timeout);
  else 
    std::cout << NETWORK_TIMEOUT << " not config, default value " 
              << settings_.network_timeout << "\n";

  const char *heart_beat_interval = config_get(&config_, HEART_BEAT_INTERVAL);
  if (heart_beat_interval) 
    settings_.heart_beat_interval = atoi(heart_beat_interval);
  else 
    std::cout << HEART_BEAT_INTERVAL << " not config, default value " 
              << settings_.heart_beat_interval << "\n";

  const char *stat_report_interval = config_get(&config_, STAT_REPORT_INTERVAL);
  if (stat_report_interval) 
    settings_.stat_report_interval = atoi(stat_report_interval);
  else 
    std::cout << STAT_REPORT_INTERVAL << " not config, default value " 
              << settings_.stat_report_interval << "\n";

  const char *lease_timeout = config_get(&config_, LEASE_TIMEOUT);
  if (lease_timeout) 
    settings_.lease_timeout = atoi(lease_timeout);
  else 
    std::cout << LEASE_TIMEOUT << " not config, default value " 
              << settings_.lease_timeout << "\n";

  const char *mysql_port = config_get(&config_, MYSQL_PORT_NO);
  if (mysql_port) 
    settings_.mysql_port = atoi(mysql_port);
  else 
    std::cout << MYSQL_PORT_NO << " not config, default value " 
              << settings_.mysql_port << "\n";

  return true;
}

static void save_pidfile(const pid_t pid, const char *pid_file) {
  FILE *fp;
  if (pid_file == NULL)
    return;

  if ((fp = fopen(pid_file, "w")) == NULL) {
    LOG(ERROR) << "Could not open the pid file " << pid_file << " for writing.";
    return;
  }

  fprintf(fp, "%ld\n", (long)pid);
  if (fclose(fp) == -1) {
    LOG(ERROR) << "Could not close the pid file " << pid_file;
    return;
  }
}

static void remove_pidfile(const char *pid_file) {
  if (pid_file == NULL)
    return;

  if (unlink(pid_file) != 0) {
    LOG(WARNING) << "Could not remove the pid file " << pid_file;
  }
  pid_file = NULL;
}

// Check the service already exists
static int check_server(const char *pid_file) {

  FILE *f = NULL;
  int pid = 0; // pid number from pid file

  if ((f = fopen(pid_file, "r")) == 0) {
    LOG(INFO) << "pid file " << pid_file << " not exist.";
    return -1;
  }

  if (fscanf(f, "%d", &pid) != 1) {
    LOG(ERROR) << "Can't read " << pid_file << " file!";
    return -1;
  }

  // send signal SIGTERM to kill
  if (pid > 0) {
    LOG(WARNING) << "Check to have launched services, kill " << pid << " !";
    kill(pid, SIGTERM);
  }

  fclose(f);
  return 0;
}

//-----------------------------------------------------------------------------
// Signal handler callback functions
//-----------------------------------------------------------------------------
static void signal_handler(const int sig) {
  if (sig != SIGTERM && sig != SIGQUIT && sig != SIGINT) {
      return;
  }
  if (true == daemon_quit_) return;

  daemon_quit_ = true;
  LOG(WARNING) << "Signal(" << sig << ") received, try to exit daemon gracefully..";

  if (true == settings_.run_as_daemon)
    remove_pidfile(settings_.pid_file);

  config_release(&config_);

  Tracker::Finalize();

  // make sure deadlock detect loop is quit
  sleep(2);

  exit(EXIT_SUCCESS);
}

static void usage_license(void) {
  std::string version = StringPrintf("%s version %d.%d.%d\n", \
                        FASTDCS_PACKAGE_NAME, kMajorVersion,  \
                        kMinorVersion, kRevisionNumber);
  printf(version.data());
  printf("Copyright (C) 2013, Liu Xun. <my@liuxun.org>\n"
        "FastDCS may be copied only under the terms of the GNU General\n"
        "Public License V3, which may be found in the FastDCS source kit.\n"
        "Please visit the FastDCS Home Page http://www.FastDCS.com/ \n"
        "for more detail.\n"
  );
}

static void usage(void) {
  std::string version = StringPrintf("%s version %d.%d.%d\n", \
                        FASTDCS_PACKAGE_NAME, kMajorVersion,  \
                        kMinorVersion, kRevisionNumber);
  printf(version.data());
  printf("Usage: "
         FASTDCS_PACKAGE_NAME
         " [options] file...\n"
         "Options:\n"
         "-f <file>    load config by <file>.\n"
         "-h           print this help and exit.\n"
         "-i           print license info and exit.\n"
        );
  printf("For bug reporting instructions, please see:\n"
         "FastDCS Home Page http://www.FastDCS.com/.\n"
        );
}

int main(int argc, char const *argv[])
{
  // register signal callback
  if (signal(SIGTERM, signal_handler) == SIG_ERR)
    std::cout << "can not catch SIGTERM.";
  if (signal(SIGQUIT, signal_handler) == SIG_ERR)
    std::cout << "can not catch SIGQUIT.";
  if (signal(SIGINT,  signal_handler) == SIG_ERR)
    std::cout << "can not catch SIGINT.";

  int c;
  char config_file[PATH_MAX];
  memset(config_file, NULL, sizeof(char)*PATH_MAX);
  
  // process arguments
  if (argc == 1) {
    usage();
    exit(EXIT_SUCCESS);
  }

  while ((c = getopt(argc, (char* const*)argv, "f:hi")) != EOF) {
    switch (c) {
      case 'f':
        sprintf(config_file, "%s", optarg);
        break;
      case 'h':
        usage();
        exit(EXIT_SUCCESS);
      case 'i':
        usage_license();
        exit(EXIT_SUCCESS);
      default:
        usage();
        exit(EXIT_SUCCESS);
    }
  }

  CHECK_EQ(LoadConfig(config_file), true);

  // Check the service already exists
  if (true == settings_.run_as_daemon)
    check_server(settings_.pid_file);

  // Switching service users
  struct passwd *pw = NULL;
  if (getuid() == 0 || geteuid() == 0) {
      if(0 == strcmp(settings_.run_by_user, "")) {
        LOG(ERROR) << "can't run as root without the run_by_user switch";
        exit(EX_USAGE);
      }
      if ((pw = getpwnam(settings_.run_by_user)) == 0) {
        LOG(ERROR) << "can't find the user " << settings_.run_by_user << " to switch to";
        exit(EX_NOUSER);
      }
      if (setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0) {
        LOG(ERROR) << "failed to assume identity of user " << settings_.run_by_user;
        exit(EX_OSERR);
      }
  }

  InitializeLogger(settings_.info_log, settings_.warn_log, 
                   settings_.err_log, settings_.log_max_size);

  // Modify the CORE dump file size limit
  if (true == settings_.max_core_file) {
    struct rlimit rlim_new;
    struct rlimit rlim;
    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
      rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
      if (setrlimit(RLIMIT_CORE, &rlim_new)!= 0) {
        // failed. try raising just to the old max
        rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
        (void)setrlimit(RLIMIT_CORE, &rlim_new);
      }
    }
    // Testing again modify the CORE dump file size limit
    if ((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0) {
      LOG(ERROR) << "failed to ensure corefile creation!";
      exit(EXIT_FAILURE);
    }
  }

  // Running in daemon mode
  if (true == settings_.run_as_daemon) {
      int res = daemonize(((settings_.max_core_file == true) ? 0 : 1), 1);
      if (res == -1) {
        LOG(ERROR) << "failed to daemon() in order to do_daemonize!";
        return 1;
      }
  }

  // save the PID in if we're a daemon
  if (true == settings_.run_as_daemon)
    save_pidfile(getpid(), settings_.pid_file);

  Tracker::Initialize(settings_);
  
  Tracker::Finalize();

  signal_handler(0);

  return 0;
}
