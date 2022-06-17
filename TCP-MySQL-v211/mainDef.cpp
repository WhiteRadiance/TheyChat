#include <cstdarg>		// vprintf()
#include "mainDef.h"


c_client::c_client() : _fd(-1), _loc(-1), _port(0), _flag_ctrlpanel_mode(false), \
	_flag_ctrlpanel_passwd(false), _buf_to_client(nullptr), _buf_from_client(nullptr), \
	_bufsize(0), _rdCursor(0)
{
	memset(_ip, 0, sizeof(_ip));
#if defined(MYSQL_DATABASE)
	_flag_logged_in = false;
	_flag_once_RdMail = false;
	_user_id = "";
#endif
}


c_client::c_client(int fd, int loc, const char *ip, uint16_t port, size_t bufsize)
	: _fd(fd), _loc(loc), _port(port), _bufsize(bufsize)
{
	if(_ip[0] != '\0')
		strcpy(_ip, ip);
	_flag_ctrlpanel_mode = false; _flag_ctrlpanel_passwd = false;
	if(bufsize == 0) {
		_buf_to_client = nullptr;
		_buf_from_client = nullptr;
	}
	else {
		_buf_to_client = new char[bufsize]();
		_buf_from_client = new char[bufsize]();
	}
	_bufsize = bufsize;
	_rdCursor = 0;
#if defined(MYSQL_DATABASE)
	_flag_logged_in = false;
	_flag_once_RdMail = false;
	_user_id = "";
#endif
}


c_client::~c_client(void)
{
//	cout << "c_client 析构函数." << endl;
	if(_buf_to_client != nullptr) {
		delete[] _buf_to_client;
		_buf_to_client = nullptr;
	}
	if(_buf_from_client != nullptr) {
		delete[] _buf_from_client;
		_buf_from_client = nullptr;
	}
}


c_client& c_client::operator = (const c_client& c)
{
	if(this != &c)		// don't need to assign myself */
	{
		_fd = c._fd;
		_loc = c._loc;
		if(c._ip[0] != '\0')
			strcpy(_ip, c._ip);
		_port = c._port;
		_flag_ctrlpanel_mode = c._flag_ctrlpanel_mode;
		_flag_ctrlpanel_passwd = c._flag_ctrlpanel_passwd;
		if(_bufsize != 0) {
			delete[] _buf_to_client;
			delete[] _buf_from_client;
		}
		if(c._bufsize != 0) {
			_buf_to_client = new char[c._bufsize]();
			_buf_from_client = new char[c._bufsize]();
			strcpy(_buf_to_client, c._buf_to_client);
			strcpy(_buf_from_client, c._buf_from_client);
		}
		else {
			_buf_to_client = nullptr;
			_buf_from_client = nullptr;
		}
		_bufsize = c._bufsize;
#if defined(MYSQL_DATABASE)
		_flag_logged_in = c._flag_logged_in;
		_user_id = c._user_id;
#endif
	}
	return *this;
}


/*
c_client(const c_client& c)
{
    cout << "client拷贝构造函数." << endl;
    _fd = c._fd;
    _loc = c._loc;
    strcpy(_ip, c._ip);
    _port = c._port;
    _flag_ctrlpanel_mode = c._flag_ctrlpanel_mode;
    _flag_ctrlpanel_passwd = c._flag_ctrlpanel_passwd;
    _buf_to_client = new char[c._bufsize]();
    _buf_from_client = new char[c._bufsize]();
    strcpy(_buf_to_client, c._buf_to_client);
    strcpy(_buf_from_client, c._buf_from_client);
    _bufsize = c._bufsize;
}
*/



/* ================================================================================ */
/* ================================================================================ */



// add a fd from qfd_add to sfd
int c_thread::add_one_fd_from_queue(void)
{
	int fd = _qfd_add.front();
	_sfd.insert(fd);
	_qfd_add.pop();
	return fd;
}


// delete a fd in sfd according to queue
int c_thread::del_one_fd_accord_queue(void)
{
	int fd = _qfd_del.front();
	_sfd.erase(fd);
	_qfd_del.pop();
	return fd;
}


int c_thread::thread_create(int loc, void *(*__start_routine)(void *))
{
	_loc = loc;
	arg_callback = 0x0000;
	pthread_mutex_init(&_mutex_add, NULL);
	pthread_mutex_init(&_mutex_del, NULL);
	pthread_cond_init(this->cond_wk(), NULL);
	pthread_cond_init(this->cond_thc(), NULL);
	pthread_mutex_init(this->mutex_wk(), NULL);
	pthread_mutex_init(this->mutex_thc(), NULL);
	return pthread_create(&_id, NULL, __start_routine, (void *)this);
}


int c_thread::thread_detach(void)
{
	_solo = 1;
	return pthread_detach(_id);
}


int c_thread::thread_cancel(void)
{
	return pthread_cancel(this->self_id());
}



/* ================================================================================ */
/* ================================================================================ */


static MSG_RANK PRINT_RANK 	= RANK_ERROR; 	// print()/fprint()的等级设置


int print(MSG_RANK msg_rk, const char *fmt, ...)
{
	if(msg_rk > PRINT_RANK) 	// don't show msg if its rank is lower (>) than global
		return 0;

	va_list args;
	va_start(args, fmt);
	int res = vprintf(fmt, args);
	va_end(args);
	return res;
}

int fprint(MSG_RANK msg_rk, FILE *stream, const char *fmt, ...)
{
	if(msg_rk > PRINT_RANK) 	// don't show msg if its rank is lower (>) than global
		return 0;

	va_list args;
	va_start(args, fmt);
	int res = vfprintf(stream, fmt, args);
	va_end(args);
	return res;
}
