#include "IO_api.h"

using namespace std;


extern map<int, c_client>   mClient;
extern vector<c_thread>     vThread;


/**
 * intro:	Parse In_cmdstr and Prepare Out_retstr
 * 
 * @cmdstr:	input command string
 * @retstr:	output response string
 * @pc: 	ptr of current c_client
 * @dbc:	ptr of current c_db_channel
 * 
 * return:	2 if send msg to a client
 *  			note: retstr is the message to sent, not a response
 *  		1 if retstr has sth.	->	next EPOLL_OUT
 *  		0 if retstr is blank	->	still EPOLL_IN
 *  	   -1 if has any errors
 * 
 * note:	this func will overwrite @retstr
 */
int IO_Parse_and_Prepare(const char *cmdstr, char *retstr, c_client *pc, c_db_channel *dbc)
{
	int res = 1;
	/* in case of: use control panel */
	if(strncmp(cmdstr, "ctrl", 4) == 0 || strncmp(cmdstr, "control", 7) == 0 || \
		strncmp(cmdstr, "Ctrl", 4) == 0 || strncmp(cmdstr, "Control", 7) == 0 || \
		pc->state_get_ctrl_mode() == true || pc->state_get_ctrl_passwd() == true)
	{
		res = ctrlpanel_FSM(cmdstr, retstr, pc);
	}
	/* in case of: query the database */
	else if(strncmp(cmdstr, "LOG ", 4) == 0 || strncmp(cmdstr, "REG ", 4) == 0 || \
		strncmp(cmdstr, "OFF ", 4) == 0)
	{
		if(dbc == NULL)
			strcpy(retstr, "MySQL is not included.\n");
		else
			res = MySQL_account_interaction(cmdstr, retstr, pc, dbc);
	}
	/* in case of: send message to sb. */
	else if(strncmp(cmdstr, "SND ", 4) == 0)
	{
		if(dbc == NULL)
			strcpy(retstr, "MySQL is not included.\n");
		else
			res = MySQL_sendmsg_interaction(cmdstr, retstr, pc, dbc);
	}
	/* in case of: html or Web app */
	else if(strncmp(cmdstr, "HTM ", 4) == 0 || strncmp(cmdstr, "WEB ", 4) == 0)
	{
		res = 0;
	}
	/* in case of: just a simple test */
	else
	{
		strcpy(retstr, "/:Ack-应答:/\n");
	}
	return res;
}




// return:	0 if recv=0, 1 if recv okay, -1 if has any error
int IO_Recv(c_client *pc, struct epoll_event *ep_event, size_t batch, const char *specify)
{
	char *ibuf = (specify != NULL) ? (char *)specify : pc->buf_from_client();
	size_t  rwlen = 0;
	int64_t tail = 0;

	while(1)
	{
		size_t rest = pc->bufsize()-rwlen;
		size_t btr = (rest > batch) ? batch : rest;
		tail = recv(ep_event->data.fd, ibuf+rwlen, btr, 0);
//		printf("read len = %d.\n", tail);
		if(-1 == tail) {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			print(RANK_ERROR, "[Error] Thread: Recv [%d]: %s (errno=%d), so we close it.\n",
									ep_event->data.fd, strerror(errno), errno);
			return -1;
		}
		else if(0 == tail) {
			/* End of file. It means the client has closed. */
			print(RANK_INFO, "[info]  Found client [%d] is close.\n", ep_event->data.fd);
			return -1;
		}
		else {
			rwlen += (size_t)tail;
			if(rwlen >= pc->bufsize()-1)	{ rwlen = pc->bufsize()-1; break; }
//			if(tail < btr)					break;
		}
	} // end of while-loop
/*
	if(rwlen == 0)						continue;	// if rdlen=0, don't change EPOLL_I/O
	else if(ibuf[rwlen-1] == '\n')		ibuf[rwlen-1] = '\0';
	else								ibuf[rwlen] = '\0';
*/
	if(rwlen == 0)						return 0;
	else if(ibuf[rwlen-1] == '\n')		ibuf[rwlen-1] = '\0';
	else								ibuf[rwlen] = '\0';
	return 1;
}




// return:	0 if send okay or len(buf)=0, -1 if has any error
int IO_Send(c_client *pc, struct epoll_event *ep_event, const char *specify)
{
	const char *obuf = (specify != NULL) ? specify : pc->buf_to_client();
	size_t osize = strlen(obuf);			// do not include '\0'
	size_t  rwlen = pc->rdCursor();			// previous wrlen or just zero
	int64_t tail = 0;

	if(osize == 0)
		return 0;

	while(1)
	{
		size_t rest = osize-rwlen;
		tail = send(ep_event->data.fd, obuf+rwlen, rest, 0);
		if(-1 == tail) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				/* 
					* msg不适合套接子的发送缓冲区(可理解为只发送了一部分),要继续注册EPOLLOUT事件
					* 来发送剩余的数据. 因为ET模式下遇到EWOULDBLOCK则不再触发了.
					*/
				print(RANK_WARN, "[warn]   server's send() meets EAGAIN|EWOULDBLOCK, try next loop.\n");
				pc->set_rdCursor(rwlen);	// record this wrlen for next try
				break;
			}
			else if(errno == EMSGSIZE) {
				/* msg太长,以至于无法通过底层协议来'原子'传递,则报EMSGSIZE并不允传递. */
				print(RANK_ERROR, "[Error] EMSGSIZE: len(msg) > protocol's limit, and is not transmitted.\n");
			}
			print(RANK_ERROR, "[Error] Thread: Send [%d]: %s (errno=%d), so we close it.\n",
									ep_event->data.fd, strerror(errno), errno);
			return -1;
		}
		else if(0 == tail) {
			/* Send is failed. */
			print(RANK_ERROR, "[Error] Thread: tail=0 when sent to client [%d]!\n", ep_event->data.fd);
			return -1;
		}
		else {
			rwlen += (size_t)tail;
			if(rwlen >= osize) {
				pc->set_rdCursor(0);	// clear wrlen as zero
				break;
			}
			if((size_t)tail < rest)	{			// may encounter a '\0' in sending len=rest
				pc->set_rdCursor(0);	// clear wrlen as zero
				break;
			}
		}
	} // end of while-loop
	return 0;
}
