#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h> 	// to block signal 'SIGPIPE'
#include <sys/epoll.h>
#include <pthread.h>

#include <iostream>
#include <algorithm>
#include <vector>
#include <map>

#include "mainDef.h"
#include "IO_api.h"
#include "CtrlPanel.h"
#include "ThreadComm.h"

#if defined(MYSQL_DATABASE)
	#include "Database.h"
#endif

/*  Use "g++ -lpthread -lmysqlclient" to link <pthread> and <libmysqlclient> DLL.			 */
/*  For simplicity, you can input cmd 'make' to auto-compile if Makefile exists.			 */

/* [架构]:
 *  		同*_v1.cpp,依然是epoll()的 '多线程IO复用模型',新增代码解耦,新增消息邮箱.
 *  		主线程负责监听和处理控制面板的请求,4个子线程负责处理普通客户端的连接.
 *  	  + 定义'MYSQL_DATABASE'可以激活MySQL相关的功能.
 * [新增]:
 *  	  & 服务器MySQL启用了自动重连
 *  	  > 对'SIGPIPE'信号进行忽略,防止进程被中止.因此包含了进程间通信的信号机制 <signal.h>
 *  	  + 解析和填充IO-buf的操作被划分到IO_api.h中去,解耦收发和处理,分离上层和底层.
 *  	  + 控制面板相关的定义和操作被划分到CtrlPanel.h中去,并被IO_api.h调用.
 *  	  + DataClassDef.h中的函数实现已移至其cpp源文件中,以防止multi-defined错误.
 *  	  + 为了实现转发消息,新增消息邮箱,位于ThreadComm.h,包含mail类,mailbox类和函数接口.
 *  	  + 用于cleanup线程的 routine_callback()也被移动至ThreadComm.h中.
 * [特点]:
 *  		本程序基于*_v1.cpp,拥有其全部特性,并追加以下:
 *  		1. 非阻塞下recv()遇到EAGAIN|EWOULDBLOCK且rdlen=0则跳过修改事件,继续保持EPOLLIN事件.
 *  		2. 非阻塞下send()遇到EAGAIN|EWOULDBLOCK代表msg不适合套接子缓冲区;EMAGSIZE代表msg过长.
 *  		3. IO_Parse_and_Prepare()返回值(=1)表明服务器对此cmd不给予应答,也保持EPOLLIN事件.
 *  	c:	4. 对应的客户端v2代码: 升级为select/epoll模型,可以接收随机出现的消息,并可以监测stdin.
 *  	c:	5. 对应的客户端v2代码: 使用epoll模型可以方便地知晓是否已与服务器端断开连接.
 *  	c:	6. 对应的客户端v2代码: 使用select模型只能read=0来间接判断与服务器端断开连接.
 * [报错]:
 *  	  & 在mainDef.h中定义了可选打印等级的print()/fprint()函数:
 *  		[info]: 正常的提示信息. 客户端正常断开会弹出info(要么不显示出来)
 *  		[warn]: 值得注意的警告. 服务器发现客户端的错误会弹出warn.
 *  		[Error]:服务器的程序运行错误,会导致异常中止.
 * 
 *  		EPOLLERR 	(0x008):	客户端自己的错误,或对已经关闭的sock读/写才会出现.
 *  		EPOLLRDHUP	(0x2000): 	客户端断开'读'通道后被识别的标记 	( Linux >= 2.6.17	)
 *  		EPOLLHUP 	(0x010): 	客户端断开'连接'后被识别到的标记 	( ALways compatible	)
 * 
 * [注意]:
 *  	  + 客户端不会发送'\0',若结尾出现'\n'也会被服务器读取时忽略,但是只忽略末尾的'\n'
 *  		因本地测试所需,控制面板可能会被分配到子线程中. 原则上 127.0.0.1 的连接都由主线程来负责处理!
 *  		Line=280 附近的代码控制着: 来自某些ip的连接会被留在主线程处理,client._loc会标记成777.
 *  		本程序没有对控制面板的部分进行加锁!
 * [同步]:
 *  		几乎所有的资源(包括互斥锁)都被定义成全局的,方便线程间的共享.
 *  		主程序拥有1把全局锁(mlient),每个子线程都包含1个条件变量(wkup)和3把锁(cond/add/del)
 *  	  + 新增全局锁(mUser, mMailbox)
 *  	  + 新增全局容器(同上)
 * [缺点]:
 *  		MySQL的连接通道 目前只有一个!
 *  		多线程模型想要做的好特别复杂,线程数量超过核心数的2倍后就不能真正的并行处理了.
 *  		不允许客户端连接在子线程之间转移.
 *  	  + 允许客户端之间发送消息,即使接收方不在线(上线时会立刻读取信箱里的留言)
 *  		线程数一旦确定就不能动态调整了,不灵活.
 * [展望]:
 *  	  X    服务器使用std::set保存账号密码,允许像QQ一样传送(转发)消息.
 *  		1. 改为:最多只有'THREADS_LIMIT'个子线程,平常按需开辟 (线程池)
 *  		2. 客户端连接应该有超时检测的,长时间没有读写操作则强制断开.
 * [版权]:
 *  		Creator = Plexer, File = Server_TCP_mature, Version > 2.00
 *  		此为个人练习之作,欢迎大家针对此程序进行交流,参考,纠错,拷贝,商用.
 *  			GitHub  -->  https://github.com/WhiteRadiance
 *  			E-mail  -->  930522968@qq.com (China Mainland)
 * [修订]:
 *  		NEW=base , ADD=append , DEL=remove , REV=revise=ADD|DEL
 *  		2022/01/06  	v2.00_NEW 		Baseline, see comments with prefix='+,c,X'
 *  		2022/03/23		v2.10_ADD 		Add autoConnect, printRank, see prefix='&'
 *  							 _NEW 		TheyChat = VS + Qt + CloudServer + GitHub
 *  		2022/05/06  	v2.11_ADD 		Ignore/block 'SIGPIPE', see prefix='>'
 */

#if defined(MYSQL_DATABASE)
	c_db_channel *dbc1 = NULL;
	extern map<string, c_user> mUser;
	extern pthread_mutex_t mutex_mUser;
#endif

extern vector<c_mailbox> 	vMailbox;
extern pthread_mutex_t 		mutex_mMailbox;


#define BUFFER_MAXSIZE		4096
#define MAXEVENTS_MAIN		4			// Maxevents returned by epoll_wait() in main-thread
#define MAXEVENTS_WORK		1024		// Maxevents returned by epoll_wait() in work-thread
#define LISTEN_QUEUESIZE	SOMAXCONN	// Max queue length specified by listen() in <socket.h>
#define RD_BATCH_SIZE		200			// use recv() to read 200 Bytes from client every time

#define THREADS_LIMIT		4			// number of child-threads, not greater than CPU-cores



std::map<int, c_client> 	mClient;				// Map: Client信息 (以其描述符来排序)auto
std::vector<c_thread> 		vThread(THREADS_LIMIT);	// Vec: Thread信息 (以当前负载来排序)sort
pthread_mutex_t 			mutex_mClient;


void  routine_callback(void *arg);
void *thread_Serve_client(void *arg);


static const char IP_DoNot_Assign[] = "192.168.3.";	// stay in main if from ip=127.0.0.*


int main()
{
	/* define necessary variables used in main-thread */
	int 				err;
	int 				nfds;
	int 				ep_fd;
	int 				serverfd;
	struct sockaddr_in	serveraddr;
	struct epoll_event	ep_events[MAXEVENTS_MAIN] = {0};
	std::vector<std::vector<int>> vfd_prepared(THREADS_LIMIT);	// to save fds array after accept()

	/* state variables only used in epoll's while-loop */
	int 				maxevents = 1;
	int 				clientfd;
	struct sockaddr_in 	clientaddr;
	socklen_t 			addr_len;

	/*						ignore/block SIGPIPE
		1.	信号机制是针对进程的,如果用在多线程中,大多是同步的用法,如sigwait()
			signal()因规范老旧,不适合用于多线程/多进程的环境下,改用sigaction()
		2.	Linux中,每个进程都有自己的mask和action,前者用于屏蔽,后者用于处理
			每个线程拥有独立的mask,子线程会继承mask,线程间共享同一套action
		3.	阻塞SIGPIPE对应mask,会导致该线程无法捕获这个信号,那么肯定也无需处理了
			忽略SIGPIPE就是不作处理,是全局的action,会导致捕获到信号后啥也不做
		4.	下面会使用2种方法来防止SIGPIPE中止进程,通过 #if/#endif 来选择方法
		调用signal()会修改进程下的所有线程,虽然我本就打算全局忽略'SIGPIPE'
		推荐用sigaction()代替signal(),因为后者规范老旧,甚至都不适应多进程 		*/
#if 0
	// by sigaction()
	//	register an action (IGNORE) of all threads
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if(sigemptyset(&sa.sa_mask)==-1 || sigaction(SIGPIPE, &sa, NULL)==-1) {
		print(RANK_ERROR, "[Error] Failed to ignore SIFPIPE: %s (errno=%d)\n", strerror(errno), errno);
		return -1;
	}
#else
	// by pthread_sigmask
	//	可用pthread_sigmask()屏蔽信号,子线程将继承这个屏蔽集合
	//	可用sigwait()暂停线程等待
	//	可用pthread_kill()来触发某个线程注册的SIGQUIT处理函数,若未注册则中止整个进程
	sigset_t maskset;
	sigemptyset(&maskset);
	sigaddset(&maskset, SIGPIPE);
	if(pthread_sigmask(SIG_BLOCK, &maskset, NULL) == -1) {
		print(RANK_ERROR, "[Error] Failed to ignore SIFPIPE: %s (errno=%d)\n", strerror(errno), errno);
		return -1;
	}
#endif

#if defined(MYSQL_DATABASE)
	dbc1 = new c_db_channel;
	c_db_channel::s_library_init();
	dbc1->init_conn();
	dbc1->real_connect();
	dbc1->set_CharSet("utf8");
	pthread_mutex_init(&mutex_mUser, NULL);
#endif

	pthread_mutex_init(&mutex_mClient, NULL);
	pthread_mutex_init(&mutex_mMailbox, NULL);
	for(int t=0; t<THREADS_LIMIT; t++)
		vfd_prepared[t].reserve(LISTEN_QUEUESIZE);

	/* create all child-threads previously (they will blocked) */
	for(int i=0; i<THREADS_LIMIT; i++)
	{
		err = vThread[i].thread_create(i, thread_Serve_client);
		if(0 != err) {
			print(RANK_ERROR, "[Error] Create thread[%d] failed, err-code=%d", i, err);
			goto RET_MAIN_DESTROY_SRC;
		}
	}
	print(RANK_MUST, "[info]  Now you have created another 4 work-threads.\n");

	/* create server's socket to listen */
	serverfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if(-1 == serverfd) {
		print(RANK_ERROR, "[Error] Create socket: %s (errno=%d)\n", strerror(errno), errno);
		goto RET_MAIN_CANCEL_THREADS;
	}
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family		= AF_INET;
	serveraddr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serveraddr.sin_port			= htons(6666);
	err = bind(serverfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if(-1 == err) {
		print(RANK_ERROR, "[Error] Bind socket: %s (errno=%d)\n", strerror(errno), errno);
		goto RET_MAIN_CLOSE_SERVER_SOCKET;
	}
	else {
		print(RANK_MUST, "[info]  Server IP: %s, Port: %d \n", inet_ntoa(serveraddr.sin_addr), \
													ntohs(serveraddr.sin_port));
	}
	err = listen(serverfd, LISTEN_QUEUESIZE);
	if(-1 == err) {
		print(RANK_ERROR, "[Error] Listen socket: %s (errno=%d)\n", strerror(errno), errno);
		goto RET_MAIN_CLOSE_SERVER_SOCKET;
	}

	/* insert listen socket into epoll Events */
	ep_fd = epoll_create(1);
	if(-1 == ep_fd) {
		print(RANK_ERROR, "[Error] Create Epoll: %s (errno=%d)\n", strerror(errno), errno);
		goto RET_MAIN_CLOSE_EPOLL;
	}
	ep_events[0].data.fd = serverfd;
	ep_events[0].events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	err = epoll_ctl(ep_fd, EPOLL_CTL_ADD, serverfd, &ep_events[0]);
	if(-1 == err) {
		print(RANK_ERROR, "[Error] Epoll ADD_server: %s (errno=%d)\n", strerror(errno), errno);
		goto RET_MAIN_CLOSE_EPOLL;
	}
	print(RANK_INFO, "  ================================================================= \n");
	print(RANK_MUST, "  ============= listening and waiting for clients ... ============= \n\n");

	while(1)
	{
		nfds = epoll_pwait(ep_fd, ep_events, maxevents, 20, NULL);		// 20ms time-out

		if(nfds == -1) {
			print(RANK_ERROR, "[Error] Epoll_wait(): %s (errno=%d)\n", strerror(errno), errno);
			goto RET_MAIN_CLOSE_EPOLL;
		}

		for(int i=0; i<nfds; i++)
		{
			if(ep_events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
			{
			/*
			 * An error has occurred on this fd, or the socket
			 * is not ready for reading.
			 */
				print(RANK_WARN, "[warn]  Epoll exceptions, event_code = 0x%X.\n", ep_events[i].events);
				if(ep_events[i].events & EPOLLRDHUP)
					print(RANK_WARN, "[info]  Found EPOLLRDHUP on the ");
				else if(ep_events[i].events & EPOLLHUP)
					print(RANK_WARN, "[info]  Found EPOLLHUP on the ");
				else if(ep_events[i].events & EPOLLERR)
					print(RANK_WARN, "[warn]  Found EPOLLERR on the ");
				else;

				print(RANK_WARN, "sock[%d]: %s (errno=%d), so we close it.\n", ep_events[i].data.fd, strerror(errno), errno);
				if(ep_events[i].data.fd == serverfd) {
					print(RANK_ERROR, "[Error] Fatal Exception! The error above is from Server_sock.\n");
					goto RET_MAIN_CLOSE_EPOLL;
				}
				else
				{
					/* remove this fd from Epoll's monitor queue and close them */
					err = epoll_ctl(ep_fd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
					if(-1 == err) {
						print(RANK_ERROR, "[Error] Epoll DEL_sock: %s (errno=%d)\n", strerror(errno), errno);
						goto RET_MAIN_CLOSE_EPOLL;
					}
					close(ep_events[i].data.fd);
					pthread_mutex_lock(&mutex_mClient);													// Lock mClient
					mClient.erase(ep_events[i].data.fd);
					pthread_mutex_unlock(&mutex_mClient);												// Unlock mClient
				}
			}
			else if(ep_events[i].data.fd == serverfd)
			{
			/*
			 * We had a notification on serverfd, which means there
			 * are some incoming requests. ( N <= MaxEvents )
			 */
				for(int t=0; t<THREADS_LIMIT; t++) {
					vfd_prepared[t].clear();
				}
				pthread_mutex_lock(&mutex_mClient);														// Lock mClient
				for(int t=0; t<THREADS_LIMIT; t++) {
					vThread[t]._load = vThread[t].sfd_size();
				}
				while(1)
				{
					clientfd = accept(serverfd, (struct sockaddr *)&clientaddr, &addr_len);
					if(-1 == clientfd) {
						// We have processed all incoming requests if EAGAIN.
						if(errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						print(RANK_ERROR, "[Error] Accept socket: %s (errno=%d)\n", strerror(errno), errno);
						goto RET_MAIN_UNLOCK_M;
					}
					else {
						print(RANK_INFO, "[info]  New connection: sockfd=%d, ip=%s, port=%d \n", clientfd, \
										inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
					}

					if(false)
						maxevents = (2 <= MAXEVENTS_MAIN) ? 2 : MAXEVENTS_MAIN;		// =2 if CtrlPanel-client is connected
					if(false)
						maxevents = (1 <= MAXEVENTS_MAIN) ? 1 : MAXEVENTS_MAIN;		// =1 if only listen socket (as normal)


					/* -----  Deliver client's socket to an proper thread  ----- */
					/*  														 */
					/*  1. choose a thread which is the most idle.               */
					/*  2. insert this socket into global mClient.               */
					/*  3. push it into global vprepared for future inserting.   */
					/*  4. Pseudo-change thread's _load for next choosing.       */

					int loc = min_element(vThread.begin(), vThread.end()) - vThread.begin();
					uint16_t port_t = ntohs(clientaddr.sin_port);
					char *client_ipaddr = inet_ntoa(clientaddr.sin_addr);
					/* 			下面这一行[插入map]操作的底层比较复杂,分析如下:
						1. 首先执行等号右边,即"带参的构造函数",内部会new开辟两个client要用的bufs
						2. 然后执行等号左边,即"无参的构造函数",内部为默认初始化,且其bufs=nullptr
						3. 最后执行等号的重载,内部基本都是赋值,不过指针是重新new再拷贝内容的哦!
						4. if{}结束时,等号右边的临时client达到生命周期,即调用析构函数自我销毁
						[注意]:编译器会有默认的拷贝构造和赋值重载,类内有指针变量时必须重写以防止浅拷贝!
					*/
					if(strncmp(client_ipaddr, IP_DoNot_Assign, 15) == 0) {
						/* don't assign to any thread, just stay in main-thread (777) 反正线程数也达不到700多~ */
						mClient[clientfd] = c_client(clientfd, 777, client_ipaddr, port_t, BUFFER_MAXSIZE);
					}
					else {
						mClient[clientfd] = c_client(clientfd, loc, client_ipaddr, port_t, BUFFER_MAXSIZE);
						vfd_prepared[loc].push_back(clientfd);
						vThread[loc]._load += 1;
					}
				}
				pthread_mutex_unlock(&mutex_mClient);													// Unlock mClient

				for(int t=0; t<THREADS_LIMIT; t++)
				{
					if(vfd_prepared[t].size() != 0)
					{
						vThread[t].thread_mutex_lock_add();												// Lock add
						for(size_t j=0; j<vfd_prepared[t].size(); j++)
							vThread[t].push_qfd_add(vfd_prepared[t][j]);
						vThread[t].thread_mutex_unlock_add();											// Unlock add

						/* signal the condvar to wake up the sleeping thread */
						vThread[t].thread_mutex_lock_cond(vThread[t].mutex_wk());						// Lock cond
						if(vThread[t].sfd_size() == 0)
							vThread[t].thread_cond_signal(vThread[t].cond_wk());						// Signal cond
						vThread[t].thread_mutex_unlock_cond(vThread[t].mutex_wk());						// Unlock cond
					}
				}
			}
			else if(ep_events[i].events & EPOLLIN)
			{
				print(RANK_ERROR, "\n[Sorry] EpollIn in main-thread is unfinished.\n");
			}
			else if(ep_events[i].events & EPOLLOUT) // asynchronous process
			{
				print(RANK_ERROR, "\n[Sorry] EpollOut in main-thread is unfinished.\n");
			}
			else ;
		} // end of for-loop
	} // end of while-loop

	return 0;


RET_MAIN_UNLOCK_M:
	pthread_mutex_unlock(&mutex_mClient);
RET_MAIN_CLOSE_EPOLL:
	close(ep_fd);
RET_MAIN_CLOSE_SERVER_SOCKET:
	close(serverfd);
RET_MAIN_CANCEL_THREADS:
	for(size_t i=0; i<vThread.size(); i++)
		vThread[i].thread_cancel();
RET_MAIN_DESTROY_SRC:
	pthread_mutex_destroy(&mutex_mMailbox);
	pthread_mutex_destroy(&mutex_mClient);
	mClient.clear();
	sleep(1);			// wait 1 sec for cancelling thread

#if defined(MYSQL_DATABASE)
	pthread_mutex_destroy(&mutex_mUser);
	dbc1->close_conn();
	delete dbc1;
	c_db_channel::s_thread_release_vars();
	c_db_channel::s_library_end();
#endif
	return -1;
}








/* --------------------------------  main / child thread  ---------------------------- */









/**
 * intro:	thread function for serving clients
 * 
 * @arg:	 pointer of a c_thread's object
 * 
 * note:	None
 */
void *thread_Serve_client(void *arg)
{
	int 				 err = 0;
	int 				 nfds = 0;
	int 				 ep_fd = 0;
	struct epoll_event	 ep_events[MAXEVENTS_WORK] = {0};
	c_client 			*pc = nullptr;		// pointer of a client in mClient

	c_thread *pThread = (c_thread *)arg;


	/* detach thread itself to auto resycle resources before it return */
	err = pThread->thread_detach();
	if(0 != err) {
		print(RANK_ERROR, "[Error] Thread: Detach thread[%d] failed, err-code=%d", pThread->loc(), err);
		goto ERR_THREAD_RETURN;
	}

	ep_fd = epoll_create(1);
	if(-1 == ep_fd) {
		print(RANK_ERROR, "[Error] Thread: Create Epoll: %s (errno=%d)\n", strerror(errno), errno);
		goto ERR_THREAD_RETURN;
	}
	/* sync isclass-epfd and the local-ep_fd, so others can get this epfd from outside. */
	pThread->sync_epfd(ep_fd);

#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_initiate_vars();
#endif

	while(1)
	{
		pthread_testcancel();		// set a thread's cancel point

		/* block itself until the condvar-wake-up is triggered. */
		pThread->arg_callback = 0x0800 + pThread->loc();
		pThread->thread_mutex_lock_cond(pThread->mutex_wk());											// Lock cond
		pthread_cleanup_push(routine_callback, (void*)&pThread->arg_callback);							// Cleanup-Push
		while(pThread->sfd_size() == 0 && pThread->add_size() == 0)
		{
			pThread->thread_cond_wait(pThread->cond_wk(), pThread->mutex_wk());							// Wait for cond
		}
		pthread_cleanup_pop(0);																			// Cleanup-Pop
		pThread->thread_mutex_unlock_cond(pThread->mutex_wk());											// Unlock cond

		/* add (update) socket into thread's sfd */
		if(pThread->add_size() != 0)
		{
			pThread->arg_callback = 0x0200 + pThread->loc();
			pThread->thread_mutex_lock_add();															// Lock add
			pthread_cleanup_push(routine_callback, (void*)&pThread->arg_callback);						// Cleanup-Push
			for(size_t i=0; i<pThread->add_size(); i++)
			{
				int sock = pThread->add_one_fd_from_queue();

				/* change socket into NON-BLOCK mode. */
				err = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
				if(-1 == err) {
					print(RANK_ERROR, "[Error] Thread: Fcntl sock_nonblock: %s (errno=%d)\n", strerror(errno), errno);
					goto ERR_THREAD_UNLOCK_A;
				}

				/* insert new sockets' fd into Epoll's monitor queue */
				ep_events[0].data.fd = sock;
				ep_events[0].events = EPOLLIN | EPOLLET | EPOLLRDHUP;
				err = epoll_ctl(ep_fd, EPOLL_CTL_ADD, sock, ep_events);
				if(-1 == err) {
					print(RANK_ERROR, "[Error] Thread: Epoll ADD_client: %s (errno=%d)\n", strerror(errno), errno);
					goto ERR_THREAD_UNLOCK_A;
				}
			}
			pthread_cleanup_pop(0);																		// Cleanup-Pop
			pThread->thread_mutex_unlock_add();															// Unlock add
		}

		nfds = epoll_pwait(ep_fd, ep_events, MAXEVENTS_WORK, 40, NULL);		// 40ms time-out

		if(nfds == -1) {
			print(RANK_ERROR, "[Error] Thread: Epoll_wait(): %s (errno=%d)\n", strerror(errno), errno);
			goto ERR_THREAD_RETURN;
		}

		/* inform clinets to read Mailbox */
		ThComm_inform_to_read_mails(pThread, ep_fd);

		for(int i=0; i<nfds; i++)
		{
			pThread->arg_callback = 0x0100;																// Cleanup-Push
			pthread_mutex_lock(&mutex_mClient);															// Lock mClient
			pthread_cleanup_push(routine_callback, (void*)&pThread->arg_callback);
			pc = &(mClient[ep_events[i].data.fd]);

			if(ep_events[i].events & EPOLLERR)
			{
				print(RANK_WARN, "[warn]  Thread: Epoll exceptions, event_code = 0x%X.\n", ep_events[i].events);
				print(RANK_WARN, "[warn]  Thread: EPOLLERR on the sock[%d]", ep_events[i].data.fd);
				print(RANK_WARN, ": %s (errno=%d), so we close it.\n", strerror(errno), errno);
				goto ERR_CLOSE_CLIENT;
			}
			if(ep_events[i].events & (EPOLLHUP | EPOLLRDHUP))
			{
				print(RANK_INFO, "[info]  Thread: client [%d] closed with ", ep_events[i].data.fd);
				if(ep_events[i].events & EPOLLRDHUP) 		print(RANK_INFO, "EPOLLRDHUP ");
				else if(ep_events[i].events & EPOLLHUP) 	print(RANK_INFO, "EPOLLHUP ");
				else ;
				print(RANK_INFO, "(event_code = 0x%X)\n", ep_events[i].events);
				goto ERR_CLOSE_CLIENT;
			}
			else if(ep_events[i].events & EPOLLIN)
			{
				memset(pc->buf_from_client(), 0, pc->bufsize());
				err = IO_Recv(pc, ep_events + i, (size_t)RD_BATCH_SIZE, NULL);
				if(-1 == err)
					goto ERR_CLOSE_CLIENT;
				else if(1 == err)
					print(RANK_MUST, " => Recv [%d]\t: %s\n", ep_events[i].data.fd, pc->buf_from_client());
				else ;

				/* Prepare the data to be sent. */
#if !defined(MYSQL_DATABASE)
				err = IO_Parse_and_Prepare(pc->buf_from_client(), pc->buf_to_client(), pc);
#else
				err = IO_Parse_and_Prepare(pc->buf_from_client(), pc->buf_to_client(), pc, dbc1);
#endif
				if(-1 == err) {
					print(RANK_ERROR, "[Error] Thread: Failed to Parse cmd on Client [%d].\n", ep_events[i].data.fd);
					goto ERR_CLOSE_CLIENT;
				}
				else if(0 == err || strlen(pc->buf_to_client()) == 0) {
					print(RANK_INFO, "[info]  no response to this CMD.\n");
					continue;	// if no response to this cmd, don't change EPOLL_I/O
				}
				/* send message (pc->buf_to_client) to a client which its sockfd=err. */
				else if(2 == err)
				{
					ThComm_transfer_msg_to_another(pThread, pc->buf_to_client());
					/* if MySQL_sendmsg_interaction() recv 'SND RDY4MBX', or you
					   try to send a mail, your mailbox will wake up and work.   */
					if(pc->state_get_ready_mail() == false)
						pc->state_set_ready_mail(true);
					strcpy(pc->buf_to_client(), "sent to mailbox.\n");
				}
				else ;

				/* Send data in the next cycle. Async-Process can improve efficiency. */
				ep_events[i].events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
				err = epoll_ctl(ep_fd, EPOLL_CTL_MOD, ep_events[i].data.fd, ep_events + i);
				if(-1 == err) {
					print(RANK_ERROR, "[Error] Thread: Client [%d] Epoll MOD_event:", ep_events[i].data.fd);
					print(RANK_ERROR, " %s (errno=%d), so we close it.\n", strerror(errno), errno);
					goto ERR_CLOSE_CLIENT;
				}
			}
			else if(ep_events[i].events & EPOLLOUT)
			{
				err = IO_Send(pc, ep_events + i, NULL);
				if(-1 == err)
					goto ERR_CLOSE_CLIENT;

				/* if process of sending is not finished, keep its Epoll_Event and try next loop. */
				if(pc->rdCursor() != 0)
					continue;
				else {
					memset(pc->buf_to_client(), 0, pc->bufsize());

					/* read mails */
					if(pc->state_get_ready_mail() == true)
						ThComm_read_mails(pThread, pc, ep_events + i);
				}
				
				/* Recv data in the next cycle. Async-Process can improve efficiency. */
				ep_events[i].events = EPOLLIN | EPOLLET | EPOLLRDHUP;
				err = epoll_ctl(ep_fd, EPOLL_CTL_MOD, ep_events[i].data.fd, ep_events + i);
				if(-1 == err) {
					print(RANK_ERROR, "[Error] Thread: Client [%d] Epoll MOD_event:", ep_events[i].data.fd);
					print(RANK_ERROR, " %s (errno=%d), so we close it.\n", strerror(errno), errno);
					goto ERR_CLOSE_CLIENT;
				}
			}
			else ;

			/* do not run code 'ERR_CLOSE_CLIENT' below if no errors happend. */
			pthread_cleanup_pop(0);																		// Cleanup-Pop
			pthread_mutex_unlock(&mutex_mClient);														// Unlock mClient
			continue;

ERR_CLOSE_CLIENT:
			pthread_mutex_unlock(&mutex_mClient);														// Unlock mClient
			pThread->thread_mutex_lock_del();															// Lock del
			pThread->push_qfd_del(ep_events[i].data.fd);
//			printf("push_qfd_del() -> sock = %d.\n", ep_events[i].data.fd);
			pThread->thread_mutex_unlock_del();															// Unlock del
		
		} // end of for-loop
		pc = nullptr;


		/* del (update) socket from thread's sfd */
		if(pThread->del_size() != 0)
		{
			pThread->arg_callback = 0x0400 + pThread->loc();
			pThread->thread_mutex_lock_del();															// Lock del
			pThread->arg_callback |= 0x0100;
			pthread_mutex_lock(&mutex_mClient);															// Lock mClient
#if defined(MYSQL_DATABASE)
			pThread->arg_callback |= 0x2000;
			pthread_mutex_lock(&mutex_mUser);															// Lock mUser
#endif
			pthread_cleanup_push(routine_callback, (void *)&pThread->arg_callback);						// Cleanup-Push
			while(pThread->del_size() != 0)
			{
//				printf("pThread->del_size() = %d \n", (int)pThread->del_size());
				int sock = pThread->del_one_fd_accord_queue();
#if defined(MYSQL_DATABASE)
				/* log-out account and reset its idnum if had logged-in. */
				if(mClient[sock].state_get_logged_in()) {
					mClient[sock].state_set_logged_in(false);
					mUser.erase(mClient[sock].user_id());
					mClient[sock].set_user_id("");
				}
#endif
				/* remove pre_fds from Epoll's monitor queue and close them. */
				err = epoll_ctl(ep_fd, EPOLL_CTL_DEL, sock, NULL);
				if(-1 == err) {
					print(RANK_ERROR, "[Error] Thread: Epoll DEL_sock[%d]: %s (errno=%d)\n", sock, strerror(errno), errno);
					goto ERR_THREAD_UNLOCK_M_D;
				}
				close(sock);
				
				mClient.erase(sock);
			}
			pthread_cleanup_pop(0);																		// Cleanup-Pop
#if defined(MYSQL_DATABASE)
			pthread_mutex_unlock(&mutex_mUser);															// Unlock mUser
#endif
			pthread_mutex_unlock(&mutex_mClient);														// Unlock mClient
			pThread->thread_mutex_unlock_del();															// Unlock del
		}

	} // end of while-loop


ERR_THREAD_RETURN:
#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_release_vars();
#endif
	return NULL;


ERR_THREAD_UNLOCK_M:
	pthread_mutex_unlock(&mutex_mClient);
#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_release_vars();
#endif
	return NULL;


ERR_THREAD_UNLOCK_D:
	pThread->thread_mutex_unlock_del();
	pThread->thread_mutex_destroy_del();
#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_release_vars();
#endif
	return NULL;


ERR_THREAD_UNLOCK_A:
	pThread->thread_mutex_unlock_add();
	pThread->thread_mutex_destroy_add();
#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_release_vars();
#endif
	return NULL;


ERR_THREAD_UNLOCK_M_D:
	pthread_mutex_unlock(&mutex_mClient);
	pThread->thread_mutex_unlock_del();
	pThread->thread_mutex_destroy_del();
#if defined(MYSQL_DATABASE)
	c_db_channel::s_thread_release_vars();
#endif
	return NULL;
}
