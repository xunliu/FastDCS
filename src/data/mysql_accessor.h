/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
//
// mysql wrapper
//
#ifndef FASTDCS_DATA_MYSQL_ACCESSOR_H_
#define FASTDCS_DATA_MYSQL_ACCESSOR_H_

#include <deque>
#include <malloc.h>
#include <memory.h>
#include <mysql.h>

#include "src/base/common.h"

namespace fastdcs {

//-----------------------------------------------------------------------------
// open_mysql_tstruct - database connection handle
//-----------------------------------------------------------------------------
typedef struct open_mysql_s{
  struct open_mysql_s *next;    // pointer to next member
  MYSQL mysql;                  // MySQL connection handle
  short busy;                   // connection busy flag
}open_mysql_t;

//-----------------------------------------------------------------------------
// MysqlConnection
//-----------------------------------------------------------------------------
class MysqlConnection
{
 public:
  MysqlConnection();
  MysqlConnection(const char* database, const char* host, \
                  const char* user, const char* password, \
                  int port);
  ~MysqlConnection();

  bool Connect(const char* database, const char* host, \
               const char* user, const char* password, \
               int port);

  open_mysql_t* GrabDB();
  bool FreeDB(open_mysql_t* odb);

 protected:
  char* database_;
  char* host_;
  char* user_;
  char* password_;
  int port_;
  open_mysql_t* open_mysql_;
};

//-----------------------------------------------------------------------------
// MysqlQuery
//-----------------------------------------------------------------------------
class MysqlResult;
class MysqlQuery
{
 public:
  MysqlQuery();
  MysqlQuery(MysqlConnection* sql_connection);
  MysqlQuery(MysqlConnection* sql_connection, char* sql);
  ~MysqlQuery();

  int TryExecute(const char* sql);
  int Execute(const char* sql);
  MysqlResult* Store();
  void FreeResult(MysqlResult* sql_result);
  void FreeResult();

  int64 InsertId();

  MYSQL_FIELD* FetchField();
  char* FetchFieldName();

  int Ping();

 protected:
  MysqlConnection* connection_;
  open_mysql_t* open_mysql_;
  MYSQL_RES* res_;
  MYSQL_ROW row_;
  short rowcount_;

 private:
  std::deque<MysqlResult*> results_;
  typedef std::deque<MysqlResult*>::iterator results_iter_t;
};

//-----------------------------------------------------------------------------
// MysqlVariable
//-----------------------------------------------------------------------------
class MysqlVariable
{
 public:
  MysqlVariable();
  MysqlVariable(uint32 i);
  MysqlVariable(int i);
  MysqlVariable(double d);
  MysqlVariable(char* s);
  MysqlVariable(const char* s);
  ~MysqlVariable();

  operator uint32();
  operator int();
  operator double();
  operator char*();
  operator const char*();

  MysqlVariable& operator = (uint32 i);
  MysqlVariable& operator = (int i);
  MysqlVariable& operator = (double d);
  MysqlVariable& operator = (char* s);
  MysqlVariable& operator = (const char* s);
 protected:
  char* value_;
//  std::string value_;

  void Release();
};

//-----------------------------------------------------------------------------
// MysqlRow
//-----------------------------------------------------------------------------
class MysqlRow
{
 public:
  MysqlRow();
  MysqlRow(MYSQL_ROW row);

  void AllowNull(bool allow = true); //return null values as empty strings ("" ) if allow == 0

  MysqlVariable operator [] (int index);

 protected:
  MYSQL_ROW row_;
  bool allow_null_;
};

//-----------------------------------------------------------------------------
// MysqlField
//-----------------------------------------------------------------------------
class MysqlField
{
 public:
  MysqlField();
  MysqlField(MYSQL_FIELD* field);

  char* GetName();
  char* GetDefaultValue();
  enum_field_types GetType();
  unsigned int GetMaxLength();

 protected:
  MYSQL_FIELD* field_;
};

//-----------------------------------------------------------------------------
// MysqlResult
//-----------------------------------------------------------------------------
class MysqlResult
{
 public:
  MysqlResult();
  MysqlResult(MysqlQuery* query, MYSQL_RES* res);
  ~MysqlResult();

  int64 RowsSize();
  MysqlRow FetchRow();

  unsigned int FieldsSize();
  MysqlField FetchField(unsigned int field_idx);

 protected:
  MysqlQuery* query_;
  MYSQL_RES* res_;
};

} // namespace fastdcs

#endif  // FASTDCS_DATA_MYSQL_ACCESSOR_H_
