#include "ThreadComm.h"


extern vector<c_thread>     vThread;
extern map<int, c_client> 	mClient;
extern map<string, c_user> 	mUser;
extern pthread_mutex_t 		mutex_mClient;
extern pthread_mutex_t 		mutex_mUser;

map<string, c_mailbox> 		mMailbox;
pthread_mutex_t 			mutex_mMailbox;



// raw = "RAW snder rcver msg"
void ThComm_transfer_msg_to_another(c_thread *pT, char *raw)
{
	char type[10] = {0};
	char snder[60] = {0};
	char rcver[60] = {0};
	map<string, c_mailbox>::iterator it;

	/* create a mail */
	sscanf(raw, "%s %s %s", type, snder, rcver);
	size_t msg_pos = (size_t)4+strlen(snder)+(size_t)1+strlen(rcver)+(size_t)1;
	c_mail mail(snder, rcver, raw, msg_pos);
	mail.append_LF(); // add a '\n' if sending mail, (not if sending cmd)

	/* find where the mailbox is, and put mail in it or wait. */
	pT->arg_callback = 0x4000;
	pthread_mutex_lock(&mutex_mMailbox);									// Lock mMailbox
	it = Mbx_find_iterator_of_mailbox(mail.rcver);
	if(it->second.isFull())
	{
		pthread_mutex_unlock(&mutex_mMailbox);
		/* block itself until the condvat-thread-comm is triggered. */
		pT->arg_callback |= 0x1000 + pT->loc();
		pT->thread_mutex_lock_cond(pT->mutex_thc());						// Lock cond
		pthread_cleanup_push(routine_callback, (void*)&pT->arg_callback);	// Cleanup-Push
		while(it->second.isFull())
		{
			pT->thread_cond_wait(pT->cond_thc(), pT->mutex_thc());			// Wait for cond
		}
		pthread_cleanup_pop(0);												// Cleanup-Pop
		pT->thread_mutex_unlock_cond(pT->mutex_thc());						// Unlock cond
		pthread_mutex_lock(&mutex_mMailbox);
	}

	/* put the mail into mailbox */
	it->second.insert_mail(mail);
	pthread_mutex_unlock(&mutex_mMailbox);									// Unlock mMailbox
	return;
}



// 遍历Thread下的<set>sfd查询idnum再查询Mailbox,若有邮件则触发其EPOLLOUT
void ThComm_inform_to_read_mails(c_thread *pT, int epfd)
{
	struct epoll_event epev;
	for(auto &ff : pT->sfd())
	{
		/* locate to the mailbox of every fd in <set>sfd */
		pT->arg_callback = 0x4000;
		pthread_mutex_lock(&mutex_mMailbox);								// Lock mMailbox
//		pthread_mutex_lock(&mutex_mClient);									// Lock mClient
		pthread_cleanup_push(routine_callback, (void*)&pT->arg_callback);	// Cleanup-Push
		c_mailbox& mbx = mMailbox[mClient[ff].user_id()];
		if(mbx.qmails.size() != 0) {
			epev.data.fd = ff;
			epev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
			int err = epoll_ctl(epfd, EPOLL_CTL_MOD, ff, &epev);
			if(-1 == err) {
				print(RANK_ERROR, "[Error] Cross-Thread transfer failed: RcvClient [%d] ", ff);
				print(RANK_ERROR, "Epoll MOD_event: %s (errno=%d).\n", strerror(errno), errno);
			}
		}
		pthread_cleanup_pop(0);												// Cleanup-Pop
//		pthread_mutex_unlock(&mutex_mClient);								// Unlock mClient
		pthread_mutex_unlock(&mutex_mMailbox);								// Unlock mMailbox
	}
	return;
}




void ThComm_read_mails(c_thread *pT, c_client *pc, struct epoll_event *ep_event)
{
	bool flag_Full = false;
	size_t batch = (size_t)MAILS_READ_BATCH;

	pT->arg_callback = 0x4000;
	pthread_mutex_lock(&mutex_mMailbox);									// Lock mMailbox
	pthread_cleanup_push(routine_callback, (void*)&pT->arg_callback);		// Cleanup-Push
	c_mailbox& mbx = mMailbox[mClient[ep_event->data.fd].user_id()];
	flag_Full = mbx.isFull() ? true : false;
	batch = (batch > mbx.qmails.size()) ? mbx.qmails.size() : batch;

	/* read and erase a batch of mails in mbx */
	for(size_t i=0; i<batch; i++)
	{
		IO_Send(pc, ep_event, mbx.qmails.front().msg.c_str());
		mbx.qmails.pop();
	}
	pthread_cleanup_pop(0);													// Cleanup-Pop
	pthread_mutex_unlock(&mutex_mMailbox);									// Unlock mMailbox
	if(flag_Full)
	{
		/* signal the condvar of a Thread which is waiting to transfer a mail */
		for(size_t t=0; t<vThread.size(); t++)
		{
			vThread[t].thread_mutex_lock_cond(vThread[t].mutex_thc());		// Lock cond
			vThread[t].thread_cond_signal(vThread[t].cond_thc());			// Signal cond
			vThread[t].thread_mutex_unlock_cond(vThread[t].mutex_thc());	// Unlock cond
		}
	}
	return;
}





map<string, c_mailbox>::iterator Mbx_find_iterator_of_mailbox(string& rcver)
{
	map<string, c_mailbox>::iterator it;

	it = mMailbox.find(rcver);
	if(it == mMailbox.end()) {
		mMailbox[rcver] = c_mailbox();
		it = mMailbox.find(rcver);
		return it;
	}
	else
		return it;
}






/**
 * intro:	callback handler function for unlocking mutex
 * 
 * @arg:	High Byte	->	#mask of the mutex
 *  		Low  Byte	->	#loc of this thread
 *  		 e.g.0x0100 = mClient_mutex   ( global mutex  )
 *  			 0x0201 = qfd_add_mutex   ( in vThread[1] )
 *  			 0x0402 = qfd_del_mutex   ( in vThread[2] )
 *  			 0x0803 = cond_wk_mutex   ( in vThread[3] )
 *  			 0x1000 = cond_thc_mutex  ( in vThread[0] )
 *  			 0x2001 = mUser_mutex     ( in vThread[1] )
 *  			 0x4002 = mMailbox_mutex  ( in vThread[2] )
 *
 * note:	None
 */
void routine_callback(void *arg)
{
	unsigned short cmd = *((unsigned short *)arg);
	unsigned char lk = (cmd >> 8) & 0x00FF;
	unsigned char id = cmd & 0x00FF;

	if(lk & 0x01) {
		pthread_mutex_unlock(&mutex_mClient);
	}

	if(lk & 0x02) {
		vThread[id].thread_mutex_unlock_add();
		vThread[id].thread_mutex_destroy_add();
	}

	if(lk & 0x04) {
		vThread[id].thread_mutex_unlock_del();
		vThread[id].thread_mutex_destroy_del();
	}

	if(lk & 0x08) { // condvar of wake-up
		vThread[id].thread_mutex_unlock_cond(vThread[id].mutex_wk());
		vThread[id].thread_mutex_destroy_cond(vThread[id].mutex_wk());
		vThread[id].thread_cond_destroy(vThread[id].cond_wk());
	}

	if(lk & 0x10) { // condvar of thread-communicate
		vThread[id].thread_mutex_unlock_cond(vThread[id].mutex_thc());
		vThread[id].thread_mutex_destroy_cond(vThread[id].mutex_thc());
		vThread[id].thread_cond_destroy(vThread[id].cond_thc());
	}

	if(lk & 0x20) {
		pthread_mutex_unlock(&mutex_mUser);
	}

	if(lk & 0x40) {
		pthread_mutex_unlock(&mutex_mMailbox);
	}

#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_release_vars();
#endif
	return;
}

