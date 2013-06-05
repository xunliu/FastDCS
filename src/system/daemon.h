/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// This implementation is a daemonize
// 
#ifndef FASTDCS_SYSTEM_DAEMON_H_
#define FASTDCS_SYSTEM_DAEMON_H_

int daemonize(int nochdir, int noclose);

#endif // FASTDCS_SYSTEM_DAEMON_H_
