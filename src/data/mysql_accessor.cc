/*******************************************************************************
 * Copyright (C) 2013 Liu Xun (my@liuxun.org)                                  *
 *                                                                             *
 * FastDCS may be copied only under the terms of the GNU General               *
 * Public License V3, which may be found in the FastDCS source kit.            *
 * Please visit the FastDCS Home Page http://www.FastDCS.com/ for more detail. *
 *******************************************************************************/
#include <stdio.h>

#include "src/base/logging.h"
#include "src/data/mysql_accessor.h"
#include "src/utils/string_codec.h"
#include "src/utils/string_printf.h"

namespace fastdcs {

//-----------------------------------------------------------------------------
// Implementation of MysqlConnection
//-----------------------------------------------------------------------------
MysqlConnection::MysqlConnection(){
  host_ = user_ = password_ = database_ = NULL;
  open_mysql_ = NULL;
}

MysqlConnection::MysqlConnection(const char* database, const char* host, \
                                 const char* user, const char* password, \
                                 int port){
  host_ = user_ = password_ = database_ = NULL;
  port_ = port;
  open_mysql_ = NULL;

  Connect(database, host,  user, password, port);
}

MysqlConnection::~MysqlConnection(){
  if (host_){
    delete[] host_;
    host_ = NULL;
  }

  if (user_){
    delete[] user_;
    user_ = NULL;
  }

  if (password_){
    delete[] password_;
    password_ = NULL;
  }
  if (database_){
    delete[] database_;
    database_ = NULL;
  }

  open_mysql_t* open_mysql = NULL;
  for (open_mysql = open_mysql_; open_mysql != NULL; open_mysql = open_mysql_->next){
    mysql_close(&open_mysql->mysql);
  }

  while (open_mysql){
    open_mysql = open_mysql_;
    open_mysql = open_mysql_->next;
    if (open_mysql->busy){
      LOG(INFO)<< "Destroying mysql object before Connect object(s)";
    }

    delete open_mysql;
  }
}

bool MysqlConnection::Connect(const char* database, const char* host, \
                              const char* user, const char* password, \
                              int port){
  if (NULL == host_){
    host_ = new char[strlen(host)+1];
    strcpy(host_, host);
  }
  if (NULL == user_){
    user_ = new char[strlen(user)+1];
    strcpy(user_, user);
  }
  if (NULL == password_){
    password_ = new char[strlen(password)+1];
    strcpy(password_, password);
  }
  if (NULL == database_){
    database_ = new char[strlen(database)+1];
    strcpy(database_, database);
  }
  port_ = port;
  return FreeDB(GrabDB());
}

open_mysql_t* MysqlConnection::GrabDB(){
  open_mysql_t* open_mysql = NULL;
  for (open_mysql = open_mysql_; open_mysql != NULL; 
       open_mysql = open_mysql->next){
    if (!open_mysql->busy)
      break;
  }

  if (!open_mysql){
    open_mysql = new open_mysql_t;
    if (!mysql_init(&open_mysql->mysql)){
      LOG(ERROR) << mysql_error(&open_mysql->mysql);
      return NULL;
    }

//    LOG(INFO) << host_ << ", " << user_ << ", " << password_ << ", " << database_;
    if (!mysql_real_connect(&open_mysql->mysql, host_, user_, 
                            password_, database_, port_, NULL, CLIENT_FOUND_ROWS)){
      LOG(ERROR) << mysql_error(&open_mysql->mysql);
      return NULL;
    }

    open_mysql->busy = 1;
    open_mysql->next = open_mysql_;
    open_mysql_ = open_mysql;
  } else {
    open_mysql->busy ++;
  }

  return open_mysql;
}

bool MysqlConnection::FreeDB(open_mysql_t* open_mysql){
  if (open_mysql) {
    open_mysql->busy = 0;
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
// Implementation of MysqlQuery
//-----------------------------------------------------------------------------
MysqlQuery::MysqlQuery(){
  connection_ = NULL;
  open_mysql_ = NULL;
  res_ = NULL;
  row_ = NULL;

  results_.resize(0);
}

MysqlQuery::MysqlQuery(MysqlConnection* connection){
  connection_ = connection;
  open_mysql_ = connection_->GrabDB();
  res_ = NULL;
  row_ = NULL;

  results_.resize(0);
}

MysqlQuery::MysqlQuery(MysqlConnection* connection, char* sql){
  connection_ = connection;
  open_mysql_ = connection_->GrabDB();
  res_ = NULL;
  row_ = NULL;

  TryExecute(sql);

  results_.resize(0);
}

MysqlQuery::~MysqlQuery() {
  for (int index = 0; index < results_.size(); ++index) {
    delete results_[index];
    results_[index] = NULL;
  }

  if (res_) {
    mysql_free_result(res_);
  }

  if (open_mysql_) {
    connection_->FreeDB(open_mysql_);
  }
}

int MysqlQuery::TryExecute(const char* sql) {
  if (connection_ && open_mysql_ && !res_) {
    if (!mysql_query(&open_mysql_->mysql, sql)) {
      return 1;
    } else {
      LOG(INFO) << "sql = " << sql;
      LOG(ERROR) << mysql_error(&open_mysql_->mysql);
    }
  }

  return 0;
}

int MysqlQuery::Execute(const char* sql) {
  if (connection_ && open_mysql_ && !res_) {
    if (TryExecute(sql)) {
      res_ = mysql_store_result(&open_mysql_->mysql);
    }
  }

  return (int)res_;
}

MysqlResult* MysqlQuery::Store() {
  if (!res_) {
    return NULL;
  }

  MysqlResult* result = new MysqlResult(this, res_);
  results_.push_back(result);

  res_ = NULL; // prevent someone else from freeing this block

  return result;
}

void MysqlQuery::FreeResult(MysqlResult* sql_result) {
  for (results_iter_t it = results_.begin(); 
        it != results_.end(); ++it){
    if (*it == sql_result) {
      delete *it;
      *it = NULL;
      results_.erase(it);
      break;
    }
  }
}

void MysqlQuery::FreeResult() {
  if (connection_ && open_mysql_ && res_) {
    mysql_free_result(res_);
    res_ = NULL;
    row_ = NULL;
  }
}

int64 MysqlQuery::InsertId() {
  if (connection_ && open_mysql_) {
    return mysql_insert_id(&open_mysql_->mysql);
  }
  return -1;
}

MYSQL_FIELD* MysqlQuery::FetchField() {
  if (connection_ && open_mysql_ && res_) {
    return mysql_fetch_field(res_);
  }

  return 0;
}

char* MysqlQuery::FetchFieldName() {
  MYSQL_FIELD* field = 0;

  if (connection_ && open_mysql_ && res_) {
    field = mysql_fetch_field(res_);
  }

  return field ? field->name : NULL; // "";
}

int MysqlQuery::Ping() {
  if (open_mysql_) {
    return mysql_ping(&open_mysql_->mysql);
  }

  return -1;
}

//-----------------------------------------------------------------------------
// Implementation of MysqlVariable
//-----------------------------------------------------------------------------
MysqlVariable::MysqlVariable() {
  value_ = NULL;
}

MysqlVariable::MysqlVariable(int i) {
  value_ = NULL;
  *this = i;
}

MysqlVariable::MysqlVariable(uint32 i) {
  value_ = NULL;
  *this = i;
}

MysqlVariable::MysqlVariable(double d) {
  value_ = NULL;
  *this = d;
}

MysqlVariable::MysqlVariable(char* s) {
  value_ = NULL;
  *this = s;
}

MysqlVariable::MysqlVariable(const char* s) {
  value_ = NULL;
  *this = (char*)s;
}

MysqlVariable::~MysqlVariable() {
  Release();
}

MysqlVariable::operator int() {
  if (!value_) {
    return 0;
  }

  return atoi(value_);
}

MysqlVariable::operator uint32() {
  if (!value_) {
    return 0;
  }

  return atol(value_);
}

MysqlVariable::operator double() {
  if (!value_) {
    return 0;
  }

  return atof(value_);  
}

MysqlVariable::operator char*() {
  return (char*)value_;
}

MysqlVariable::operator const char*() {
  return value_;
}

MysqlVariable& MysqlVariable::operator = (int i) {
  Release();
  value_ = new char[32 + 1];
  sprintf(value_, "%d", i);

  return *this;
}

MysqlVariable& MysqlVariable::operator = (uint32 i) {
  Release();
  value_ = new char[32 + 1];
  sprintf(value_, "%lu", (unsigned long)i);

  return *this;
}

MysqlVariable& MysqlVariable::operator = (double d) {
  Release();
  value_ = new char[32];
  sprintf(value_, "%15.5f", d);
  return *this;
}

MysqlVariable& MysqlVariable::operator = (char* s) {
  Release();

  value_ = new char[strlen(s)+ 1];
  strcpy(value_, s);
  return *this;
}

MysqlVariable& MysqlVariable::operator = (const char* s) {
  *this = (char* )s;
  return *this;
}

void MysqlVariable::Release() {
  if (value_) {
    delete[] value_;
    value_ = NULL;
  }
}

//-----------------------------------------------------------------------------
// Implementation of MysqlRow
//-----------------------------------------------------------------------------
MysqlRow::MysqlRow() {
  row_ = NULL;
  allow_null_ = false;
}

MysqlRow::MysqlRow(MYSQL_ROW row) {
  row_ = row;
  allow_null_ = false;
}

void MysqlRow::AllowNull(bool allow) {
  allow_null_ = allow;
}

MysqlVariable MysqlRow::operator [] (int index) {
  return MysqlVariable(allow_null_ ? (row_[index]) : (row_[index] ? row_[index] : ""));
}

//-----------------------------------------------------------------------------
// Implementation of MysqlField
//-----------------------------------------------------------------------------
MysqlField::MysqlField() {
  field_ = NULL;
}

MysqlField::MysqlField(MYSQL_FIELD* field) {
  field_ = field;
}

char* MysqlField::GetName() {
  return field_->name;
}

char* MysqlField::GetDefaultValue() {
  return field_->def;
}

enum_field_types MysqlField::GetType() {
  return field_->type;
}

unsigned int MysqlField::GetMaxLength() {
  return field_->max_length;
}

//-----------------------------------------------------------------------------
// Implementation of MysqlResult
//-----------------------------------------------------------------------------
MysqlResult::MysqlResult() {
  query_ = NULL;
  res_ = NULL;
}

MysqlResult::MysqlResult(MysqlQuery* query, MYSQL_RES* res) {
  query_ = query;
  res_ = res;
}

MysqlResult::~MysqlResult() {
  if (res_) {
    mysql_free_result(res_);
  }
}

int64 MysqlResult::RowsSize() {
  if (query_ && res_) {
    return mysql_num_rows(res_);
  }

  return 0;
}

MysqlRow MysqlResult::FetchRow() {
  MYSQL_ROW row = mysql_fetch_row(res_);

  return MysqlRow(row);
}

unsigned int MysqlResult::FieldsSize() {
  if (query_ && res_) {
    return mysql_num_fields(res_);
  }

  return 0;
}

MysqlField MysqlResult::FetchField(unsigned int index) {
  if (query_ && res_) {
    MYSQL_FIELD* field = mysql_fetch_field_direct(res_, index);
    return MysqlField(field);
  }

  return MysqlField();
}

}  // namespace fastdcs
