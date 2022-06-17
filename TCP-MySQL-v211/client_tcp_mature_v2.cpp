#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
//#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

/*
 * 本程序未使用C++特有的头文件
 * 只是披了件C++的外衣
 * 
 * v2版本使用select模型
 */

#define BUFFER_MAXSIZE 	4096


int 	sockfd;
char 	ibuf[BUFFER_MAXSIZE];
char 	obuf[BUFFER_MAXSIZE];


int main(int argc, char *argv[])
{
	int err;
	int tail;                       // indicate current '\0' position of buf[4096]
	struct sockaddr_in serveraddr;
//	struct sockaddr_in clientaddr;
	char ipaddr[16] = {0};

	fd_set 			rset;
	struct timeval 	tv;
	int 			nfds = 0;


	if(argc > 2) {
		printf("[Error] Too many arguments.\n");
		printf("[Usage] ./client  <ipaddr> \n");
		return -1;
	}
	else if(argc == 2) {
		strncpy(ipaddr, argv[1], (strlen(argv[1]) >= sizeof(ipaddr)) ? sizeof(ipaddr) : strlen(argv[1]));
	}
	else {
		strcpy(ipaddr, "0.0.0.0");
		printf("[warn]  The <target> server's ip is set as default (0.0.0.0) \n");
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == sockfd) {
		printf("[Error] Create socket: %s (errno: %d)\n", strerror(errno), errno);
		return -1;
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	err = inet_pton(AF_INET, ipaddr, &serveraddr.sin_addr);
	if(err <= 0) {
		printf("[error] In inet_pton(): server ipaddr (%s) wrong.\n", ipaddr);
		goto ERR_CLOSE_SOCK;
	}
	serveraddr.sin_family 	= AF_INET;
	serveraddr.sin_port 	= htons(6666);

	err = connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if(-1 == err) {
		printf("[Error] Try to connect %s: %s (errno: %d)\n", ipaddr, strerror(errno), errno);
		goto ERR_CLOSE_SOCK;
	}
	printf("Connected.\n");
	printf("You can send msg to server:\n\n");


	while(1)
	{
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		FD_SET(0, &rset);		// stdin (fd=0)
		tv.tv_sec = 3;			// + 3 sec time-out
		tv.tv_usec = 11000;		// + 11 ms time-out

		nfds = select(sockfd+1, &rset, NULL, NULL, &tv);

		if(nfds == -1)
		{
			printf("[Error] Select(): %s (errno=%d)\n", strerror(errno), errno);
			goto ERR_CLOSE_SOCK;
		}
		else if(nfds >= 1)
		{
			if(FD_ISSET(sockfd, &rset))
			{
				/* receive message from server */
				tail = recv(sockfd, ibuf, BUFFER_MAXSIZE-1, 0);
				if(-1 == tail) {
					printf("[Error] Recv msg: %s (errno: %d)\n", strerror(errno), errno);
					goto ERR_CLOSE_SOCK;
				}
				else if(tail == 0) {
					printf("[Error] Recv EOF. Disconnected from server.\n");
					goto ERR_CLOSE_SOCK;
				}
				else ;
				ibuf[tail] = '\0';
				printf("%s", ibuf);
				fflush(stdout);
			}
			else if(FD_ISSET(0, &rset))
			{
				/* get inputs from stdin */
				fgets(obuf, BUFFER_MAXSIZE, stdin);
				tail = (int)strlen(obuf);
				if(tail > 0)
				{
					if(obuf[0] == '\n' && obuf[1] == '\0')	// don't send if only '\n'
						;
					else {
						if(obuf[tail - 1] == '\n')			// don't send '\n' to server
							obuf[tail - 1] = '\0';
						
						if(strncmp(obuf, "close", 5) == 0)
							goto ERR_CLOSE_SOCK;

						/* send message to server */
						err = send(sockfd, obuf, tail, 0);
						if(-1 == err) {
							printf("[Error] Send msg: %s (errno: %d)\n", strerror(errno), errno);
							goto ERR_CLOSE_SOCK;
						}
					}
				}
			}
			else ;
		}
		else
		{
//			printf(".");
//			fflush(stdout);
		}
	}

ERR_CLOSE_SOCK:
	usleep(10);
	close(sockfd);
	return 0;
}
