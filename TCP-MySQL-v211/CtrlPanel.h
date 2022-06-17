#ifndef _CTRLPANEL_H
#define _CTRLPANEL_H

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <map>
#include <vector>
#include "mainDef.h"


// password to enter the control panel
static const char CtrlPanel_passwd[] = "5201314";



/*
 * respond with retstr according to cmdstr
 */
void ctrlpanel_interact(const char *cmdstr, char *retstr);

/*
 * finite state machine of CtrlPanel
 */
int ctrlpanel_FSM(const char *cmdstr, char *retstr, c_client *pc);




static const char ctrlpanel_str_hi[] = \
		"\nWelcome to the server's Control Panel.\n"
		"Type 'help' for more, or 'quit' to escape.\n\n";
static const char ctrlpanel_str_help[] = \
		"\nControl Panel Interactive Commands (CPIC), version 2.11\n"
		"These commands are defined below. Type 'help' to see this list.\n\n"
		" list -c/lients\t : to list all of clients' info.\n"
		" list -t/hreads\t : to list all of threads' info.\n"
		" quit/esc\t : to escape from CPIC manual.\n\n";
static const char ctrlpanel_str_quit[] = \
		"Bye.\n\n";
static const char ctrlpanel_str_wrong[] = \
		"Unknown command. Try 'help'.\n";
static const char ctrlpanel_str_lsclient[] = \
		"\t Table of Clients:\n"
		"--------------------------------------\n"
		" sock         ip           port   loc \n"
		"--------------------------------------\n";
static const char ctrlpanel_str_lsthread[] = \
		"\t Table of Threads:\n"
		"--------------------------------------\n"
		" loc          id           load       \n"
		"--------------------------------------\n";


#endif
