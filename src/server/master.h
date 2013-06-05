/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
// 
// Master scheduling control for Management Server
// 
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <vector>

#include "src/base/logging.h"
#include "src/utils/split_string.h"
#include "src/server/tracker.h"

#ifndef FASTDCS_SERVER_MASTER_H_
#define FASTDCS_SERVER_MASTER_H_

using namespace std;
using namespace fastdcs;
using namespace fastdcs::server;

namespace fastdcs {
namespace server {

class Master : public Tracker {
public:
  static void Initialize(struct settings_s settings);
  static void Finalize();

  /*virtual*/ void InitialTracker(struct settings_s settings);
  /*virtual*/ void FinalizeTracker();
	/*virtual*/ bool ProtocolProcess(int socket_fd, TrackerProtocol tracker_proto);
	/*virtual*/ bool HeartBeat();
  /*virtual*/ bool AskTracker();

	// Election of Master Tracker Server
	bool Election();

	// Announcement of election results
	bool Coordinator(string tracker_addr);

	// Continue to pass communications protocol
	void ContinueTransferProto(fastdcs::TrackerProtocol tracker_proto);
	
	// Ask Tracker server status
	bool MasterReportStatus();
  // 新的Master服务加入Master服务组
  bool JoinMasterGroup(TrackerProtocol tracker_proto);

	// Closed the circle
	bool ClosedCircle(TrackerProtocol tracker_proto);

  // Upgrade from Master service into a Master Tracker service
	bool UpgradeTracker(); 

  // Copy synchronization job and task data from Primary Service
  bool RequestTaskDuplicate();
  // Primary service sends a copy job and task data
  bool SendTaskDuplicate(TrackerProtocol tracker_proto, int receiver_fd); 
  // Secondary services save a copy job and task data
  bool SaveTaskDuplicate(TrackerProtocol tracker_proto); 

  bool GetIdleTask(TrackerProtocol &tracker_proto);
  bool TaskCompleted(TrackerProtocol tracker_proto);

  void PrintEnv();

  // Export function of the user-defined computing tasks have been completed
  virtual bool ExportTaskUDF(vector<FdcsTask> tasks) { return false; };
  // Import function of the user-defined tasks that need to be computing
  virtual bool ImportTaskUDF(vector<FdcsTask> &tasks) { return false; };

 private:
  void ConnectOnlineMaster(string all_master);
  // add Fdcs_time to TrackerProtocol
  void AddProtoFdcsTime(TrackerProtocol &tracker_proto);

 protected:
  // task process
  static void* CallTaskExchange(void *pvoid);
  void TaskExchangeThread(void *pvoid);
  pthread_t   task_exchange_tid_;
  Mutex       mutex_; // Protect all above data and conditions.
  ConditionVariable cond_task_exchange_;  // Data exchange for variables
  void TaskExchangeSignal() { cond_task_exchange_.Signal(); };
  ConditionVariable cond_master_tracker_;  // Data exchange for variables
  void MasterSignal() { cond_master_tracker_.Signal(); };
};

} // namespace server
} // namespace fastdcs

#endif  // FASTDCS_SERVER_MASTER_H_
