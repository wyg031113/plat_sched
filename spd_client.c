#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "debug.h"
#include "plat_sched.h"

#define TASK_LEN 2048
char text[TASK_LEN];
char voice[TASK_LEN];
struct pres_task *tsk_text = (struct pres_task*)text;
struct pres_task *tsk_voice = (struct pres_task*)voice;
void inc_task()
{
	int i;
	int len = (rand()%(20))+1;
	tsk_text->se.len = len+sizeof(struct pres)+sizeof(struct detail);
	tsk_text->se.flag = S_DATA;
	tsk_text->pr.dst_tel_code++;
	tsk_text->pr.src_tel_code++;
	tsk_text->pr.len = len + sizeof(struct detail);
	tsk_text->de.time = time(NULL);
	tsk_text->de.type = D_TYPE_TEXT;
	tsk_text->de.sub_type++;
	tsk_text->de.speaker++;
	tsk_text->de.connector++;
	tsk_text->de.len = len;
	uint8 c = 0;
	for(i = 0; i < len; i++)
		tsk_text->de.data[i] = c++;
	*tsk_voice = *tsk_text;
	tsk_text->de.type = D_TYPE_VOICE;
}
int main()
{
	start_tcp_client();
	int cnt = 300000;
	while(cnt--)
	{
		if(!is_busy())
			inc_task();
		if(!is_busy())
			submit_task((char*)tsk_text, tsk_text->se.len + sizeof(struct sess), NULL);
		usleep(10000);
		if(!is_busy())
			submit_task((char*)tsk_voice, sizeof(struct pres_task), "voice.dat");
		usleep(10000);
	}
	stop_tcp_client();
	return 0;
}
