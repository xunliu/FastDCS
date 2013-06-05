/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string>
#include <string.h>
#include <sys/stat.h>

#include "src/base/logging.h"


int64_t Logger::log_max_size_;
std::ofstream Logger::info_log_file_;
std::ofstream Logger::warn_log_file_;
std::ofstream Logger::erro_log_file_;
std::string Logger::info_log_filename_;
std::string Logger::warn_log_filename_;
std::string Logger::erro_log_filename_;

void InitializeLogger(const std::string& info_log_filename,
                      const std::string& warn_log_filename,
                      const std::string& erro_log_filename,
                      int64_t log_max_size) {
  Logger::info_log_filename_ = info_log_filename;
  Logger::warn_log_filename_ = warn_log_filename;
  Logger::erro_log_filename_ = erro_log_filename;
  Logger::log_max_size_ = log_max_size;
  Logger::info_log_file_.open(info_log_filename.c_str(), std::ofstream::app);
  Logger::warn_log_file_.open(warn_log_filename.c_str(), std::ofstream::app);
  Logger::erro_log_file_.open(erro_log_filename.c_str(), std::ofstream::app);

  srand((int)time(0));
}

/*static*/
std::ostream& Logger::GetStream(LogSeverity severity) {
  return (severity == INFO) ?
      (info_log_file_.is_open() ? info_log_file_ : std::cout) :
      (severity == WARNING ?
       (warn_log_file_.is_open() ? warn_log_file_ : std::cerr) :
       (erro_log_file_.is_open() ? erro_log_file_ : std::cerr));
}

/*static*/
std::ostream& Logger::Start(LogSeverity severity,
                            const std::string& file,
                            int line,
                            const std::string& function) {
  time_t now = 0;
  struct tm now_tm = {0};
  char time_string[128];

  unsigned int tid = (unsigned int)pthread_self() % 1000;

  time(&now);
  localtime_r(&now, &now_tm);
  sprintf(time_string, "%02d-%02d %02d:%02d:%02d", \
          now_tm.tm_mon+1, now_tm.tm_mday, \
          now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);

  std::string short_file = file;
  if (file.length() > 20) {
    short_file = ".." + short_file.substr(file.length()-20, 20);
  }

  if (rand()%1000 == 1)
    TruncateLogFile(severity);

  return GetStream(severity) << "[tid-" << tid << "]" 
                             << time_string << " " << short_file <<  "." 
                             << function << "(" << line << "): " << std::flush;
}

Logger::~Logger() {
  GetStream(severity_) << "\n" << std::flush;

  if (severity_ == FATAL) {
    info_log_file_.close();
    warn_log_file_.close();
    erro_log_file_.close();
    abort();
  }
}

/*static*/
void Logger::TruncateLogFile(LogSeverity severity) {
  const char *path = NULL;
  struct stat statbuf;
  const int kCopyBlockSize = 8 << 10;
  char copybuf[kCopyBlockSize];
  int64_t read_offset, write_offset;
  int64_t limit = Logger::log_max_size_;
  int64_t keep = Logger::log_max_size_/2;
  int pos = 0;

  if (severity == INFO) {
    path = info_log_filename_.c_str();
  } else if (severity == WARNING) {
    path = warn_log_filename_.c_str();
  } else {
    path = erro_log_filename_.c_str();
  }

  // Don't follow symlinks unless they're our own fd symlinks in /proc
  int flags = O_RDWR;
  const char *procfd_prefix = "/proc/self/fd/";
  if (strncmp(procfd_prefix, path, strlen(procfd_prefix))) flags |= O_NOFOLLOW;

  int fd = open(path, flags);
  if (fd == -1) {
    if (errno == EFBIG) {
      // The log file in question has got too big for us to open. The
      // real fix for this would be to compile logging.cc (or probably
      // all of base/...) with -D_FILE_OFFSET_BITS=64 but that's
      // rather scary.
      // Instead just truncate the file to something we can manage
      if (truncate(path, 0) == -1) {
        std::cerr << "Unable to truncate " << path << "\n";
      } else {
        std::cerr << "Truncated " << path << " due to EFBIG error\n";
      }
    } else {
      std::cerr << "Unable to open " << path << ", errno = " << errno << "\n";
    }
    return;
  }

  if (fstat(fd, &statbuf) == -1) {
    std::cerr << "Unable to fstat()\n";
    goto out_close_fd;
  }

  // See if the path refers to a regular file bigger than the
  // specified limit
  if (!S_ISREG(statbuf.st_mode)) goto out_close_fd;
  if (statbuf.st_size <= limit) goto out_close_fd;
  if (statbuf.st_size <= keep) goto out_close_fd;

  // This log file is too large - we need to truncate it
  std::cout << "Truncating " << path << " to " << keep << " bytes" << "\n";

  // Copy the last "keep" bytes of the file to the beginning of the file
  read_offset = statbuf.st_size - keep;

  write_offset = 0;
  int bytesin, bytesout;

  // find return line
  bytesin = pread(fd, copybuf, sizeof(copybuf), read_offset);
  for (pos = 0; pos < bytesin; pos ++) {
    if (copybuf[pos] == '\n') {
      pos += sizeof(char);
      break;
    }
  }
  read_offset += pos;

  while ((bytesin = pread(fd, copybuf, sizeof(copybuf), read_offset)) > 0) {
    bytesout = pwrite(fd, copybuf, bytesin, write_offset);
    if (bytesout == -1) {
      std::cerr << "Unable to write to " << path << "\n";
      break;
    } else if (bytesout != bytesin) {
      std::cerr << "Expected to write " << bytesin << ", wrote " << bytesout << "\n";
    }
    read_offset += bytesin;
    write_offset += bytesout;
  }
  if (bytesin == -1) 
    std::cerr << "Unable to read from " << path << "\n";

  // Truncate the remainder of the file. If someone else writes to the
  // end of the file after our last read() above, we lose their latest
  // data. Too bad ...
  if (ftruncate(fd, write_offset) == -1) {
    std::cerr << "Unable to truncate " << path << "\n";
  }
  info_log_file_.seekp(write_offset);

 out_close_fd:
  close(fd);
}
