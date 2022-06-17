#include "Database.h"

/*
 * desc:	KEY=S200101174, 'c_user' contains its sockfd and name
 * note:	this <map> saves online users, or is offline if not exist
 */
map<string, c_user> mUser;
pthread_mutex_t 	mutex_mUser;

//extern pthread_mutex_t mutex_mClient;




static char *Capital_first(char *str)
{
	if(str[0] >= 'a' && str[0] <= 'z') // Capital letter first
		str[0] -= 32;
	return str;
}


static string& Capital_first(string& str)
{
	if(str[0] >= 'a' && str[0] <= 'z') // Capital letter first
		str[0] -= 32;
	return str;
}




/**
 * intro:	query the MySQL database and respond (Part 1)
 * 			used for account-related interaction
 * @cmdstr:	input command string
 * @retstr:	output response string
 * @pc: 	ptr of current c_client
 * @dbc:	ptr of current c_db_channel
 * 
 * return:	1, 0 or -1. See 'IO_Parse_and_Prepare()'
 * 
 * note:	@retstr is not raw data but response
 *  		name in @cmdstr can't contain spaces
 */
int MySQL_account_interaction(const char *cmdstr, char *retstr, c_client *pc, c_db_channel *dbc)
{
	int err = 1;
	char split_name[61] = {0};
	char split_passwd[41] = {0};


	if(strncmp(cmdstr, "LOG ", 4) == 0)					// branch: LOG+name+passwd
	{
		if(pc->state_get_logged_in() == true) {
			strcpy(retstr, "Please try to Log-out before Log-in.\n");
			return 1;
		}
		if(strlen(cmdstr) == 4 || cmdstr[4] == ' ')
			goto ERR_UNKNOWN_LOG;
		sscanf(cmdstr+4, "%60s", split_name);
		/* if the input doesn't meet requirements */
		if((int)strlen(cmdstr)-4-(int)strlen(split_name) <= 1)
			goto ERR_UNKNOWN_LOG;
		if(cmdstr[4+strlen(split_name)+1] == ' ')
			goto ERR_UNKNOWN_LOG;
		if(cmdstr[4+strlen(split_name)] != ' ' || \
			(int)strlen(cmdstr)-4-(int)strlen(split_name)-1 >= (int)sizeof(split_passwd))
			return -1;
		strcpy(split_passwd, cmdstr + 4 + strlen(split_name) + 1);

		/* 下方命令里的binary为二元校对(可以用于区分大小写) */
		char prefix[64] = {0};
		char suffix[300] = {0};
		char q[384] = {0};
		snprintf(prefix, sizeof(prefix), "%s%s", "select ID,NAME,AKA,PASSWD from ", tblname);
		snprintf(suffix, sizeof(suffix), "%s%s%s%s%s%s%s", "ID='", split_name, "' or binary NAME='", split_name,  "' or AKA='", split_name, "'");
		snprintf(q, sizeof(q), "%s%s%s", prefix, " where ", suffix);

		dbc->query(q);
		err = dbc->store_result_local();
		if(err == 0)			{ strcpy(retstr, "missed\n");	err = 1; }
		else if(err == 1)		{ strcpy(retstr, "affect\n");	err = 1; }
		else if(err == -1)		{ return err; }
		else
		{
#if 1
// 查找多个条目
			bool flag_correct = false;
			while(dbc->fetch_row())
			{
				if(strcmp(split_passwd, dbc->row((int)dbc->num_fields()-1)) == 0) {
					flag_correct = true;
					pthread_mutex_lock(&mutex_mUser);							// Lock mUser
					if(mUser.find(string(dbc->row(0))) == mUser.end()) {
						pc->state_set_logged_in(true);
						pc->set_user_id(dbc->row(0));
						mUser[pc->user_id()] = c_user(dbc->row(0), dbc->row(1), pc->fd(), 0);
						sprintf(retstr, "%s%s%s", "correct ", dbc->row(0), ", PLZ-B-RDY-4-MBX.\n");
					}
					else
						strcpy(retstr, "repeat\n"); // this account is logged-in before
					pthread_mutex_unlock(&mutex_mUser);							// Unlock mUser
					break;
				}
			}
			dbc->free_result();
			if(!flag_correct)
				strcpy(retstr, "wrong\n");
#else
// 查找单个条目
			dbc->fetch_row();
			/* flag and record variables if success. */
			if(strcmp(split_passwd, dbc->row(2)) == 0) {
				pc->state_set_logged_in(true);
				pc->set_user_id(dbc->row(0));
				pthread_mutex_lock(&mutex_mUser);								// Lock mUser
				if(mUser.find(string(dbc->row(0))) != mUser.end()) {
					mUser[string(dbc->row(0))] = c_user(dbc->row(0), dbc->row(1), pc->fd(), 0);
					strcpy(retstr, "correct\n");
				}
				else
					strcpy(retstr, "repeat\n"); // this account is logged-in before
				pthread_mutex_unlock(&mutex_mUser);								// Unlock mUser
			}
			else
				strcpy(retstr, "wrong\n");
			dbc->free_result();
#endif
		}
	}
	else if(strncmp(cmdstr, "REG ", 4) == 0)			// branch: REG+name+passwd
	{
		if(strlen(cmdstr) == 4 || cmdstr[4] == ' ')
			goto ERR_UNKNOWN_REG;
		sscanf(cmdstr+4, "%60s", split_name);
		Capital_first(split_name);
		/* if the input doesn't meet requirements */
		if((int)strlen(cmdstr)-4-(int)strlen(split_name) <= 1)
			goto ERR_UNKNOWN_REG;
		if(cmdstr[4+strlen(split_name)+1] == ' ')
			goto ERR_UNKNOWN_LOG;
		if(cmdstr[4+strlen(split_name)] != ' ' || \
			(int)strlen(cmdstr)-4-(int)strlen(split_name)-1 >= (int)sizeof(split_passwd))
			return -1;
		strcpy(split_passwd, cmdstr + 4 + strlen(split_name) + 1);

		// 暂时只能从'MySQL服务器'那边进行注册工作
		strcpy(retstr, "Register is forbidden.\n");
	}
	else if(strncmp(cmdstr, "OFF ", 4) == 0)			// branch: OFF+line+name
	{
		if(pc->state_get_logged_in() == false) {
			strcpy(retstr, "Only had Logged-in can you Log-out.\n");
			return 1;
		}
		if(strncmp(cmdstr, "OFF line ", 9) == 0)
		{
			if((int)strlen(cmdstr)-8 >= (int)sizeof(split_name))
				return -1;
			if((int)strlen(cmdstr)-8 == 0)
				goto ERR_UNKNOWN_OFF;
			string idnum_t(cmdstr+9);
			Capital_first(idnum_t);

			/* flag and reset variables if success */
			if(idnum_t == pc->user_id()) {
				pthread_mutex_lock(&mutex_mUser);								// Lock mUser
				mUser.erase(pc->user_id());
				pthread_mutex_unlock(&mutex_mUser);								// Unlock mUser
				pc->state_set_logged_in(false);
				pc->set_user_id("");
				sprintf(retstr, "account='%s' log out.\n", idnum_t.c_str());
			}
			else
				strcpy(retstr, "wrong\n");
		}
		else
			goto ERR_UNKNOWN_OFF;
	}
	else
	{}

	return (err == -1) ? err : 1;

ERR_UNKNOWN_LOG:
	strcpy(retstr, "Command not found, did you mean:   'LOG idnum passwd'\n");
	return 1;
ERR_UNKNOWN_REG:
	strcpy(retstr, "Command not found, did you mean:   'REG idnum passwd'\n");
	return 1;
ERR_UNKNOWN_OFF:
	strcpy(retstr, "Command not found, did you mean:   'OFF line idnum'\n");
	return 1;
}




/**
 * intro:	query the MySQL database and respond (Part 2)
 *  		used for sending-msg-related interaction
 * @parm:	Same as Part 1 (above func)
 * return:	2, 1, 0 or -1. See 'IO_Parse_and_Prepare()'
 * note:	@retstr isn't response but message!
 *  		name in @cmdstr can't contain spaces
 */
int MySQL_sendmsg_interaction(const char *cmdstr, char *retstr, c_client *pc, c_db_channel *dbc)
{
	char snder[61] = {0};
	char rcver[61] = {0};
	char *msg = retstr; // 'msg' is better to understand, since it isn't a response.
	map<string, c_user>::iterator it;


	if(strcmp(cmdstr, "SND RDY4MBX") == 0)				// branch: ready to read mbx forever
	{
		if(pc->state_get_ready_mail() == false)
			pc->state_set_ready_mail(true);
		return 0;
	}
	/*
	 * Command format:	SND RAW [receiver] [data]
	 * Message format:	RAW [sender] [receiver] [data]	( if logged-in	)
	 *  				RAW debugger [receiver] [data]	( if logged-out	)
	 */
	else if(strncmp(cmdstr, "SND RAW ", 8) == 0)		// branch: SND+RAW+snder+rcver+data
	{
		int ret = 1;
		if(strlen(cmdstr) == 8 || cmdstr[8] == ' ')
			goto ERR_UNKNOWN_SND_RAW;
		/* split sender */
		sscanf(cmdstr+8, "%60s", snder);
		Capital_first(snder);
		if(cmdstr[8+strlen(snder)] != ' ' || \
			(pc->state_get_logged_in() == false && strcmp(snder, "Debugger") != 0) || \
			(pc->state_get_logged_in() == true && strcmp(snder, pc->user_id().c_str()) != 0)) {
			strcpy(retstr, "wrong sender\n");
			return 1;
		}
		if((int)strlen(cmdstr)-8-(int)strlen(snder) <= 1)
			goto ERR_UNKNOWN_SND_RAW;
		/* split receiver */
		sscanf(cmdstr+8+strlen(snder)+1, "%60s", rcver);
		Capital_first(rcver);
		if(strcmp(snder, rcver) == 0) {
			strcpy(retstr, "You can't send a msg to yourself.\n");
			return 1;
		}
#if 1
		/* verify if rcver's idnum is valid. */
		if(!MySQL_verify_idnum(rcver, dbc)) {
			strcpy(retstr, "wrong rcver\n");
			return 1;
		}
#endif
		/* if msg is null. */
		if((int)strlen(cmdstr)-8-(int)strlen(snder)-1-strlen(rcver) <= 1)
			return 0;

		/* the rest of cmdstr that hasn't been read is just the raw data. */
		sprintf(msg, "%s %s %s", "RAW", snder, rcver);
		strcat(msg, " ");
		strcpy(msg+strlen(msg), cmdstr+8+strlen(snder)+1+strlen(rcver)+1);
#if 0
		/* try to find the rcv sockfd in the online <map> mUser. */
		pthread_mutex_lock(&mutex_mUser);										// Lock mUser
		it = mUser.find(string(rcver));
		if(it != mUser.end()) {
			ret = it->second.fd();	// return fd(>2) if successful
		}
		else {
			strcpy(retstr, "The user you sent is offline, please try later.\n");
			ret = 1;
		}
		pthread_mutex_unlock(&mutex_mUser);										// Unlock mUser
#else
		ret = 2;
#endif
		return ret;
	}
	else
		goto ERR_UNKNOWN_SND_RAW;

ERR_UNKNOWN_SND_RAW:
	strcpy(retstr, "Command not found, did you mean:   'SND RAW snder rcver data'\n");
	return 1;
}




// verify whether the 'idnum' exists in the databse.
bool MySQL_verify_idnum(char *idnum, c_db_channel *dbc)
{
	int err = 0;
	char prefix[64] = {0};
	char suffix[256] = {0};
	char q[320] = {0};
	snprintf(prefix, sizeof(prefix), "%s%s%s", "select ID from ", tblname, " where ");
	snprintf(suffix, sizeof(suffix), "%s%s%s", "binary ID='", idnum, "'");
	snprintf(q, sizeof(q), "%s%s", prefix, suffix);

	dbc->query(q);
	err = dbc->store_result_local();
	if(err == 2) {
		dbc->free_result();
		return true;
	}
	else
		return false;
}





/* ============================================================================ */
/* ----------------------  In-class Function Definition  ---------------------- */
/* ============================================================================ */



bool c_db_channel::hasError()
{
	if(*mysql_error(&_conn))	return true;
	else						return false;
}


bool c_db_channel::init_conn(bool show_err)
{
	if(_flag_inited == true) {
		if(show_err == true)
			print(RANK_WARN, "[warn]  Multi-initialization of mysql-connection.\n");
		mysql_close(&_conn);
		_flag_inited = false;
	}
	if(NULL == mysql_init(&_conn)) {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL init_conn(): %s\n", mysql_error(&_conn));
		return false;
	} else {
		// 为连接预设一些额外的选项
		my_bool auto_reconnect = 1;
		mysql_options(&_conn, MYSQL_OPT_RECONNECT, &auto_reconnect);//允许后,调用_ping()失败则会自动重连
		mysql_options(&_conn, MYSQL_INIT_COMMAND, "SET NAMES utf8");//自动重连时会自动执行这里预设的命令

		_flag_inited = true;
		return true;
	}
}


bool c_db_channel::ping(void)
{
	if(!mysql_ping(&_conn)) return true;	// may auto-reconnected
	else 					return false;	// connection is dead
}


bool c_db_channel::real_connect(bool show_err)
{
	if(!mysql_real_connect(&_conn, host, user, passwd, dbname, port, NULL, 0)) {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL real_connect(): %s\n", mysql_error(&_conn));
		return false;
	} else {
		print(RANK_MUST, "[info]  Connected to MySQL Server (localhost:%u)", port);
		print(RANK_MUST, " with libclient %s \n", mysql_get_client_info());
		return true;
	}
}


bool c_db_channel::set_CharSet(const char *cs, bool show_err)
{
	if(mysql_set_character_set(&_conn, cs)) {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL set_CharSet(): %s\n", mysql_error(&_conn));
		return false;
	} else {
		if(show_err == true)
			print(RANK_MUST, "[info]  MySQL current CharSet is set to \"%s\".\n", this->get_CharSet());
		return true;
	}
}


bool c_db_channel::query(const char *cmd, bool show_err)
{
	// _ping()确认和MySQL的连接是否还活着,可能会触发自动重连
	if(!this->ping()) {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL ping(): %s\n", mysql_error(&_conn));
		return false;
	}

	this->thread_mutex_lock_dbc();															// Lock dbc
	_flag_locked = true;
	if(mysql_query(&_conn, cmd) == 0)
		return true;
	else {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL query(): %s\n", mysql_error(&_conn));
		this->thread_mutex_unlock_dbc();													// Unlock dbc
		_flag_locked = false;
		return false;
	}
}


// if len=0, function will call strlen() inside
bool c_db_channel::real_query(const char *cmd, ulong len, bool show_err)
{
	// 可能会触发自动重连
	if(!this->ping()) {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL ping(): %s\n", mysql_error(&_conn));
		return false;
	}

	this->thread_mutex_lock_dbc();															// Lock dbc
	_flag_locked = true;
	if(mysql_real_query(&_conn, cmd, len ? len : strlen(cmd)) == 0)
		return true;
	else {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL real_query(): %s\n", mysql_error(&_conn));
		this->thread_mutex_unlock_dbc();													// Unlock dbc
		_flag_locked = false;
		return false;
	}
}


/**
 * intro:	request result and store all rows in local
 * return:	 2 if can read;	(fields>0, rows>0)
 *  		 1 if affected; 	(fileds=0, rows>=0)
 *  		 0 if mis-matched;	(fields>0, rows=0)
 *  		-1 if any error
 * note:	will update variables: _num_fields, _num_rows
 */
int c_db_channel::store_result_local(bool show_err)
{
	int err = -1;
	mysql_free_result(_res);
	_res = mysql_store_result(&_conn);

	if(_res) {							// there are rows of result
		_num_fields = mysql_num_fields(_res);
		_num_rows = mysql_num_rows(_res);

		if(_num_rows == 0)	err = 0;	// pre_cmd is SELECT and was mismatched
		else				err = 2;	// next you can read the result
	}
	else {								// stored nothing, but shuould it have?
		_num_fields = mysql_field_count(&_conn);
		if(_num_fields == 0) {			// pre_cmd was not a SELECT
			_num_rows = mysql_affected_rows(&_conn);
			if(show_err == true)
				print(RANK_INFO, "[info]  there are %lld rows affected.\n", _num_rows);
			err = 1;
		}
		else {							// should have returned rows of result
			_num_rows = 0;
			if(show_err == true)
				fprint(RANK_ERROR, stderr, "[Error] MySQL store_result(): %s\n", mysql_error(&_conn));
			err = -1;
		}
	}
	this->thread_mutex_unlock_dbc();														// Unlock dbc
	_flag_locked = false;
	return err;
}


/*
 * intro:	init a result retieval instead of storing it
 * note:	1. you must (only) call fetch_row() until it return 'NULL'
 *  		2. won't change any of in-class variables
 */
bool c_db_channel::use_result_byrow(bool show_err)
{
	mysql_free_result(_res);
	_res = mysql_use_result(&_conn);

	if(_res)	return true;
	else {
		if(show_err == true)
			fprint(RANK_ERROR, stderr, "[Error] MySQL use_result(): %s\n", mysql_error(&_conn));
		return false;
	}
}


/*
 * usage:	while(fetch_row())
 *  		{
 *  			ulong *len = fetch_length();
 *  			for(uint i=0; i<num_fields; i++)
 *  				printf("[%.*s] ", (int)len[i], row[i] ? row[i] : "NULL");
 *  			printf("\n");
 *  		}
 * 
 * note:	_store() return NULL: 	just reach the end of result.
 *  		_use() return NULL: 	no more to retrieve or an error.
 */
bool c_db_channel::fetch_row(bool show_err)
{
// refer to note, check error outside this function.
	_row = mysql_fetch_row(_res);
	if(_row)	return true;
	else		return false;
}


const char *c_db_channel::row(int idx, bool show_err)
{
	if(idx >= 0 && idx <= (int)_num_fields - 1)
		return _row[idx];
	else {
		if(show_err == true)
			print(RANK_ERROR, "[Error] arg1 'idx' surpass the max size of Row[].\n");
		return NULL;
	}
}


const int c_db_channel::len(int idx, bool show_err)
{
	if(idx >= 0 && idx <= (int)_num_fields - 1)
		return (int)(_len[idx]);
	else {
		if(show_err == true)
			print(RANK_ERROR, "[Error] arg1 'idx' surpass the max size of Len[].\n");
		return -1;
	}
}


void c_db_channel::free_result()
{
	if(_res)
		mysql_free_result(_res);
	_res = NULL;
	_row = NULL;
	if(_flag_locked == true) {
		this->thread_mutex_unlock_dbc();													// Unlock dbc
		_flag_locked = false;
	}
}


void c_db_channel::close_conn()
{
	this->free_result();
	if(_flag_inited)
		mysql_close(&_conn);
	_flag_inited = false;
}


c_db_channel::c_db_channel() : _res(NULL), _row(NULL), \
	_num_fields(0), _num_rows(0), _len(NULL), _flag_inited(false), _flag_locked(false)
{
	this->thread_mutex_init();
}


c_db_channel::~c_db_channel()
{
    if(_res != NULL) {
		mysql_free_result(_res);
		_res = NULL;
	}
	if(_flag_inited == true) {
		mysql_close(&_conn);
		_flag_inited = false;
	}
	this->thread_mutex_destroy_dbc();
}


