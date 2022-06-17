#ifndef _THREADCOMM_H
#define _THREADCOMM_H

#include "mainDef.h"
#include "Database.h"
#include "IO_api.h"
#include "string.h"
#include <sys/epoll.h>
#include <map>
using namespace std;
/*
 *  每个有效的用户理应拥有一个信箱(MailBox), 其中的'有效'指的是:
 *  SND命令在Database/MySQL_sendmsg_interaction()中保证收发用户都是已注册的.
 *  邮件(Mail)会保存在字典mMailbox中, KEY=id_num
 *  给Stu777发送Mail会同时判断他是否在线, 如果:
 *  	Y: 触发他所在sock的EPOLLOUT,通知其取查看邮箱. (可能会跨线程)
 *  	N: 放在信箱即可,他上线会自动查看的.
 *  邮箱并不被 c_Client, c_user 或 c_thread 持有, 因为无论在线与否,邮箱都属于每
 *  个已在MySQL进行过注册的用户, 即使他下线了.
 */

#define MAX_MAILS_PER_MBX 	250
#define MAILS_READ_BATCH	1	//20


// callback handler function for unlocking mutex
void routine_callback(void *arg);



// mail
class c_mail
{
public:
	string 	snder;
	string 	rcver;
	string 	msg;
	size_t 	offset;

	c_mail() : snder(""), rcver(""), msg(""), offset(0) {}

	c_mail(const char *s, const char *r, const char *m, size_t pos)
		: snder(s?s:""), rcver(r?r:""), msg(m?m:""), offset(pos) {}

	void append_LF(void)	// LF: line-feed or new-line
	{
		if(!msg.empty())
			if(msg.back() != '\n')
				msg += '\n';
	}
};



// mailbox
class c_mailbox
{
public:
	queue<c_mail> 	 qmails;

	bool isFull() 		{ return (qmails.size() >= MAX_MAILS_PER_MBX); }
	void insert_mail(c_mail& mail) { qmails.push(mail); }
};





void ThComm_transfer_msg_to_another(c_thread *pT, char *raw);
void ThComm_inform_to_read_mails(c_thread *pT, int epfd);
void ThComm_read_mails(c_thread *pT, c_client *pc, struct epoll_event *ep_event);


map<string, c_mailbox>::iterator Mbx_find_iterator_of_mailbox(string& rcver);




#endif
