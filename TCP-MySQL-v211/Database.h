#ifndef _DATABASE_H
#define _DATABASE_H

#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mainDef.h"
#include <map>
#include <string>
using namespace std;


typedef unsigned char 	uchar;
typedef unsigned char 	uint8;
typedef unsigned short 	uint16;
typedef unsigned int 	uint;
typedef unsigned int 	uint32;
typedef unsigned long 	uint64;
typedef unsigned long 	ulong;


static const char 	host[] = "localhost";
static const char 	user[] = "RemoteUser_1";
static const char 	passwd[] = "123456";
static const char 	dbname[] = "plexer";
static const char 	tblname[] = "account_login";
static const uint 	port = 3306;



class c_db_channel
{
private:
	MYSQL 			 _conn;			// a connection
	MYSQL_RES 		*_res;			// result of cmd
	MYSQL_ROW 		 _row;			// a row of result
	uint 			 _num_fields;	// cols of result
	my_ulonglong 	 _num_rows;		// rows of result stored/affected
	ulong 			*_len;			// len[] of every element in a row 
	bool 			 _flag_inited;	// _conn was initiated

	bool 			 _flag_locked;	// the Mutex is locked
	pthread_mutex_t  _mutex_dbc;	// to protect the result of query()


public:
	/* static member func */
	static void s_library_init() 			{ mysql_library_init(0, NULL, NULL); }
	static void s_library_end() 			{ mysql_library_end(); }
	static void s_thread_initiate_vars() 	{ mysql_thread_init(); }
	static void s_thread_release_vars() 	{ mysql_thread_end(); }

	bool hasError();
	const char *strerr() 					{ return mysql_error(&_conn); }
	
	bool init_conn(bool show_err=true);
	bool ping();
	bool real_connect(bool show_err=true);
	bool set_CharSet(const char *cs, bool show_err=true);
	const char *get_CharSet() 				{ return mysql_character_set_name(&_conn); }
	bool query(const char *cmd, bool show_err=true);
	bool real_query(const char *cmd, ulong len=0, bool show_err=true);
	int store_result_local(bool show_err=true);
	bool use_result_byrow(bool show_err=true);
	bool fetch_row(bool show_err=true);
	void fetch_length()						{ _len = mysql_fetch_lengths(_res); }
	uint num_fields() const 				{ return _num_fields; }
	const char *row(int idx, bool show_err=true);
	const int len(int idx, bool show_err=true);
	void free_result();
	void close_conn();

	c_db_channel();
	~c_db_channel();

	/* functions based on Mutex or Semaphore */
	int thread_mutex_init() 		{ return pthread_mutex_init(&_mutex_dbc, NULL); }
	int thread_mutex_lock_dbc() 	{ return pthread_mutex_lock(&_mutex_dbc); }
	int thread_mutex_unlock_dbc() 	{ return pthread_mutex_unlock(&_mutex_dbc); }
	int thread_mutex_destroy_dbc() 	{ return pthread_mutex_destroy(&_mutex_dbc); }
};



/*
 * Interaction with MySQL database
 */
int MySQL_account_interaction(const char *cmdstr, char *retstr, c_client *pc, c_db_channel *dbc);
int MySQL_sendmsg_interaction(const char *cmdstr, char *retstr, c_client *pc, c_db_channel *dbc);


bool MySQL_verify_idnum(char *idnum, c_db_channel *dbc);



/* 保存当前在线的用户信息 */
class c_user
{
private:
	string 	_id_num;		// is also the KEY of map<string, c_user>
	string 	_name;
	int 	_fd;
	uchar 	_status;		// FakeOff/Quiet/Busy/...


public:
	string id_num() 			{ return _id_num; }
	string name() 				{ return _name; }
	const int fd() const		{ return _fd; }
	const uchar status() const 	{ return _status; }
	bool isOnLine() 			{ return (_status == 1); }
//	bool isOffLine() 			{ return (_status == 0); }

	c_user(const char *idn=NULL, const char *name=NULL, int fd=-1, uchar status=0)
		: _id_num(idn?idn:""), _name(name?name:""),_fd(fd), _status(status) {}
};


#endif
