#ifndef _IO_API_H
#define _IO_API_H

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <map>
#include <vector>
#include "mainDef.h"
#include "CtrlPanel.h"
#include "Database.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>


int IO_Parse_and_Prepare(const char *cmdstr, char *retstr, c_client *pc, \
                        c_db_channel *dbc=NULL);

int IO_Recv(c_client *pc, struct epoll_event *ep_event, size_t batch, \
                        const char *specify=NULL);

int IO_Send(c_client *pc, struct epoll_event *ep_event, \
                        const char *specify=NULL);


#endif
