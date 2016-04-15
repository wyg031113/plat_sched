/*************************************************************************
    > File Name: pres_demo.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2016年04月02日 星期六 18时28分49秒
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "debug.h"
#include "plat_sched.h"
#define BUF_SIZE 1024
char sig_buf[BUF_SIZE];
char voice_buf[BUF_SIZE];

struct control_sig *cs = (struct control_sig*)sig_buf;
struct voice *vc = (struct voice*)voice_buf;

int snd_control_sig()
{
	static unsigned int d = 0;
	struct control_sig mycs;
	mycs.type = VC_TYPE;
	mycs.cmd = CS_CMD;
	mycs.time = time(NULL);
	mycs.data = d%5;
	d++;
	return put_sig(&mycs);
}
int snd_voice_sig()
{
	uint8 c = 0;
	int i;
	int len = rand()%(BUF_SIZE - sizeof(struct voice));
	vc->type = VC_TYPE;
	vc->cmd = V_CMD;

	vc->len = len;
	for(i = 0; i < len; i++)
		vc->data[i] = c++;
	return put_voice(vc);
}

void check_ret(int ret)
{
	if(ret != PS_SUCCESS)
	{
		DEBUG("SND  FAILED!\n");
		if(ret == EBUFF_FULL)
			DEBUG("\033[1;33m BUFFER FULL\033[0m \n");
	}
}
int main1()
{
	int cnt = 10000000;
	int ret;
	start_sig_voice();
	while(cnt--)
	{
		//sleep(1);
		INFO("\033[1;33m main1\033[0m \n");
		memset(sig_buf, 0, sizeof(sig_buf));
		memset(voice_buf, 0, sizeof(voice_buf));
		int msg_type = get_msg_type();
		if(msg_type == MSG_SIGNAL)
		{
			get_sig(cs);
			DEBUG("\nRecv SIG: type:%x command:%x time:%x data:%x\n",
					cs->type, cs->cmd, cs->time, cs->data);
			continue;
		}
		else if(msg_type == MSG_VOICE)
		{
			get_voice(vc, BUF_SIZE);
			DEBUG("\nRecv voice: type:%x command:%x len=0x%x\n",
					vc->type, vc->cmd, vc->len);
			uint8 c = 0;
			int i = 0;
			for(i = 0; i < vc->len; i++)
				if(vc->data[i] != c++)
				{
					INFO("Bad recv voice data at %dth byte\n", i);
					exit(1);
				}
			continue;
		}

		//if(get_msg_type() == MSG_NOMSG)
		{
			ret = snd_control_sig();
			check_ret(ret);

			ret = snd_voice_sig();
			check_ret(ret);
			usleep(50000);
		}
		//DEBUG("Main loop.\n\n");
	}
	sleep(1);
	stop_sig_voice();
	return 0;
}
