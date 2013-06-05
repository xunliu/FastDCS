/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include "src/base/common.h"
#include "src/utils/string_util.h"

using std::string;

string StringReplace(const string& input,
										 const string& find,
										 const string& replaceWith) { 
	std::string out(input);
	int curPos = 0;

	int pos;
	while((pos = out.find(find, curPos)) != -1) {
	  out.replace(pos, find.size(), replaceWith); // 一次替换
	  curPos = pos + replaceWith.size();					// 防止循环替换!
	}

	return out;
}

int ParseBytes(const char *str_bytes, const int default_value, int64 *bytes) {
	char *pReservedEnd = NULL;

	*bytes = strtol(str_bytes, &pReservedEnd, 10);
	CHECK_GT(*bytes, 0);

	if (pReservedEnd == NULL || *pReservedEnd == '\0') {
		*bytes *= default_value;
	} else if (*pReservedEnd == 'G' || *pReservedEnd == 'g') {
		*bytes *= 1024 * 1024 * 1024;
	} else if (*pReservedEnd == 'M' || *pReservedEnd == 'm') {
		*bytes *= 1024 * 1024;
	} else if (*pReservedEnd == 'K' || *pReservedEnd == 'k') {
		*bytes *= 1024;
	}

	return 0;
}