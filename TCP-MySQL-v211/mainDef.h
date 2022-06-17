#ifndef _MAINDEF_H
#define _MAINDEF_H

#include <cstdio>
#include <cstring>		// to use memset()
#include <pthread.h>

#include <iostream>
#include <set>
#include <queue>
using namespace std;


// Support MySQL Functions
#define MYSQL_DATABASE



class c_client
{
private:
	int 		 _fd;
	int 		 _loc;
	char 		 _ip[20];
	uint16_t 	 _port;

	bool 		 _flag_ctrlpanel_mode;	// into a state of operating control panel
	bool 		 _flag_ctrlpanel_passwd;// into a state of waiting for passwd
#if defined(MYSQL_DATABASE)
	bool 		 _flag_logged_in;		// has logged in and can chat with sb.
	bool 		 _flag_once_RdMail;		// ready to read mbx  ( if recv 'SND RDY4MBX' )
	string 		 _user_id;
#endif
	char 		*_buf_to_client;		// string to be sent TO client
	char 		*_buf_from_client;		// string to be recv FROM client
	size_t 		 _bufsize;				// record the size of above two bufs
	size_t 		 _rdCursor;				// 发送量超出TCP协议缓存时,用于记录下次发送时的起始点


public:
	int fd() const 	{ return _fd; }
	int loc() const {return _loc; }
	char *ip() 		{ return _ip; }
	char *buf_to_client() 	{ return _buf_to_client; }
	char *buf_from_client() { return _buf_from_client; }
	size_t bufsize() const 	{ return _bufsize; }
	uint16_t port() const 	{ return _port; }
	size_t rdCursor() const { return _rdCursor; }
	void set_rdCursor(size_t pos) { _rdCursor = pos; }

	void state_set_ctrl_mode(bool y=true) 	{ _flag_ctrlpanel_mode = y; }
	bool state_get_ctrl_mode() const 		{ return _flag_ctrlpanel_mode; }
	void state_set_ctrl_passwd(bool y=true) { _flag_ctrlpanel_passwd = y; }
	bool state_get_ctrl_passwd() const 		{ return _flag_ctrlpanel_passwd; }
#if defined(MYSQL_DATABASE)
	void state_set_logged_in(bool y=true) 	{ _flag_logged_in = y; }
	bool state_get_logged_in() const 		{ return _flag_logged_in; }
	void state_set_ready_mail(bool y=true) 	{ _flag_once_RdMail = y; }
	bool state_get_ready_mail() const 		{ return _flag_once_RdMail; }
	string& user_id()						{ return _user_id; }
	void set_user_id(const char *u) 		{ _user_id = u; }
	void set_user_id(const string& u) 		{ _user_id = u; }
#endif
	bool operator < (const c_client& right) const { return _fd < right._fd; }

	c_client();
	c_client(int fd, int loc, const char *ip, uint16_t port, size_t bufsize);
	~c_client();

	c_client& operator = (const c_client& c);	// Operator '=' overload of class <c_client>
	c_client(const c_client& c) = delete;		// delete: no default Copy-Constructor even if no user's
};







class c_thread
{
private:
	pthread_t 		_id;
	int 			_loc;				// locates in somewhere of Thread Pool, not used
	set<int>        _sfd;
	bool 			_solo;				// indicated whether the thread is detached
	queue<int> 		_qfd_add;
	queue<int> 		_qfd_del;			// pre_del fds from thread
	int 			_ep_fd;				// fd of its epoll

	pthread_cond_t 	_cond_wakeup;		// used to wake up sleeping thread
	pthread_cond_t 	_cond_thcomm;		// used to communicate across threads
	pthread_mutex_t _mutex_cond_wk;
	pthread_mutex_t _mutex_cond_thc;
	pthread_mutex_t _mutex_add;
	pthread_mutex_t _mutex_del;


public:
	size_t 			_load;			// is used to pre-allocates the accepted fds
	unsigned short 	arg_callback;	// argument of routine_callback() to canceling a thread

	pthread_t self_id(void) const 	{ return _id; }
	int loc(void) const 			{ return _loc; }
	int epfd(void) const 			{ return _ep_fd; }
	void sync_epfd(int epfd) 		{ _ep_fd = epfd; }
	set<int>& sfd(void) 			{ return _sfd; }
	size_t sfd_size(void) const { return _sfd.size(); }
	size_t add_size(void) const { return _qfd_add.size(); }
	size_t del_size(void) const { return _qfd_del.size(); }
	bool IsDetached(void) const 	{ return _solo; }
    int add_one_fd_from_queue(void);
    int del_one_fd_accord_queue(void);
	void push_qfd_add(int fd) 		{ _qfd_add.push(fd); }
	void pop_qfd_add() { _qfd_add.pop(); }
	void push_qfd_del(int fd) 		{ _qfd_del.push(fd); }
	void pop_qfd_del() { _qfd_del.pop(); }

	bool operator < (const c_thread& right) const { return _load < right._load; }

	c_thread() : _id(0), _loc(-1), _solo(false), _ep_fd(-1), _load(0) {}
	c_thread(pthread_t id, int loc, bool solo=0, int epfd=-1, size_t load=0)
			: _id(id), _loc(loc), _solo(solo), _ep_fd(epfd), _load(load) {}
	
	c_thread(const c_thread& t) = delete;

	/* top level functions based on <pthread.h> */
	int thread_create(int loc, void *(*__start_routine)(void *));
	int thread_detach(void);
	int thread_cancel(void);
	pthread_cond_t 	*cond_wk() 		{ return &_cond_wakeup; }
	pthread_cond_t 	*cond_thc() 	{ return &_cond_thcomm; }
	pthread_mutex_t *mutex_wk() 	{ return &_mutex_cond_wk; }
	pthread_mutex_t *mutex_thc() 	{ return &_mutex_cond_thc; }
	int thread_mutex_lock_add(void) 		{ return pthread_mutex_lock(&_mutex_add); }
	int thread_mutex_unlock_add(void) 		{ return pthread_mutex_unlock(&_mutex_add); }
	int thread_mutex_destroy_add(void) 		{ return pthread_mutex_destroy(&_mutex_add); }
	int thread_mutex_lock_del(void) 		{ return pthread_mutex_lock(&_mutex_del); }
	int thread_mutex_unlock_del(void) 		{ return pthread_mutex_unlock(&_mutex_del); }
	int thread_mutex_destroy_del(void) 		{ return pthread_mutex_destroy(&_mutex_del); }
	int thread_cond_wait(pthread_cond_t *co, pthread_mutex_t *mu) 	{ return pthread_cond_wait(co, mu); }
	int thread_cond_signal(pthread_cond_t *co) 						{ return pthread_cond_signal(co); }
	int thread_cond_destroy(pthread_cond_t *co) 					{ return pthread_cond_destroy(co); }
	int thread_mutex_lock_cond(pthread_mutex_t *mu) 				{ return pthread_mutex_lock(mu); }
	int thread_mutex_unlock_cond(pthread_mutex_t *mu) 				{ return pthread_mutex_unlock(mu); }
	int thread_mutex_destroy_cond(pthread_mutex_t *mu) 				{ return pthread_mutex_destroy(mu); }
};






enum MSG_RANK {
	RANK_MUTE 	= 0,
	RANK_ERROR,
	RANK_WARN,
	RANK_NOTE,
	RANK_INFO,
//	RANK_DEBUG,
	
	RANK_MUST 	= RANK_ERROR,
	RANK_OFF 	= RANK_MUTE,
	RANK_ALL 	= RANK_INFO
};



int print(MSG_RANK msg_rk, const char *fmt, ...); 					// used in almost everywhere
int fprint(MSG_RANK msg_rk, FILE *stream, const char *fmt, ...); 	// used in 'Database.cpp'


#endif
