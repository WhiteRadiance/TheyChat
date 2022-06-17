#include "CtrlPanel.h"

using namespace std;


extern map<int, c_client> 	mClient;
extern vector<c_thread> 	vThread;


/**
 * intro:	respond with retstr according to cmdstr
 * 
 * @cmdstr:	input command string
 * @retstr:	output response string
 * 
 * note:	this func will overwrite @retstr
 */
void ctrlpanel_interact(const char *cmdstr, char *retstr)
{
	char temp[256] = {0};		// used for sprintf()
	if(cmdstr == nullptr)
		strcpy(retstr, "cmd is null-string!\n");
	else if(strcmp(cmdstr, "help") == 0)
		strcpy(retstr, ctrlpanel_str_help);
	else if(strcmp(cmdstr, "list clients") == 0 || \
			strcmp(cmdstr, "list -clients") == 0 || \
			strcmp(cmdstr, "list -c") == 0)
	{
		strcpy(retstr, ctrlpanel_str_lsclient);
		for(auto &mc : mClient) {
			sprintf(temp, "  %-3d\t  %-15s  %-5d   %d\n", mc.first, mc.second.ip(), mc.second.port(), mc.second.loc());
			strcat(retstr, temp);
			memset(temp, 0, sizeof(temp));
		}
		strcat(retstr, "\n");
	}
	else if(strcmp(cmdstr, "list threads") == 0 || \
			strcmp(cmdstr, "list -threads") == 0 || \
			strcmp(cmdstr, "list -t") == 0)
	{
		strcpy(retstr, ctrlpanel_str_lsthread);
		for(auto &vt : vThread) {
			sprintf(temp, "  %d\t 0x%lX     %-3lu\n", vt.loc(), vt.self_id(), vt.sfd_size());
			strcat(retstr, temp);
			memset(temp, 0, sizeof(temp));
		}
		strcat(retstr, "\n");
	}
	else if(strcmp(cmdstr, "hi") == 0 || strcmp(cmdstr, "hello") == 0)
		strcpy(retstr, ctrlpanel_str_hi);
	else if(strcmp(cmdstr, "quit") == 0 || strcmp(cmdstr, "esc") == 0)
		strcpy(retstr, ctrlpanel_str_quit);
	else
		strcpy(retstr, ctrlpanel_str_wrong);

	return;
}




/**
 * intro:	finite state machine of CtrlPanel
 * 
 * @cmdstr:	input command string
 * @retstr:	output response string
 * @pc: 	ptr of current c_client
 * 
 * return:	always return 1. See 'IO_Parse_and_Prepare()'
 * 
 * note:	this func will overwrite @retstr
 */
int ctrlpanel_FSM(const char *cmdstr, char *retstr, c_client *pc)
{
	if(pc->state_get_ctrl_mode() == true)			// state: still in CCIP mode
	{
		if(strcmp(cmdstr, "quit") == 0 || strncmp(cmdstr, "esc", 3) == 0) {
			ctrlpanel_interact(cmdstr, retstr);
			pc->state_set_ctrl_mode(false);
			pc->state_set_ctrl_passwd(false);
		}
		else
			ctrlpanel_interact(cmdstr, retstr);
	}
	else
	{
		if(pc->state_get_ctrl_passwd() == true)		// state: wait for password
		{
			if(strcmp(cmdstr, CtrlPanel_passwd) == 0) {
				ctrlpanel_interact("hi", retstr);
				pc->state_set_ctrl_mode(true);
				pc->state_set_ctrl_passwd(false);
			}
			else if(strcmp(cmdstr, "quit") == 0 || strncmp(cmdstr, "esc", 3) == 0) {
				ctrlpanel_interact(cmdstr, retstr);
				pc->state_set_ctrl_mode(false);
				pc->state_set_ctrl_passwd(false);
			}
			else
				strcpy(retstr, "Wrong passwd, try again: ");
		}
		else										// state: input "ctrl/control"
		{
			pc->state_set_ctrl_passwd(true);
			strcpy(retstr, "Input password: ");
		}
	}
	return 1;
}

