#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h>

#include "debug.h"
#include "plat_sched.h"

#define TASK_LEN 2048000
int offset = 0;
char rcv_buf[TASK_LEN];
int npkt_send = 0;
int npkt_recv = 0;
void show_binary(char *buf, int len)
{
	//-----------------
	//
	//
		return;
	int i;
	char c = 0;
	for(i = 0; i < len; i++)
	{

		printf("%x ", (unsigned char)buf[i]);
		CHECK2(buf[i] == c++);
	}
	printf("\n");
}

int show(char *buf, int len)
{
	int i;
	struct pres_task *pt = (struct pres_task*)buf;
	INFO("pkt_len=%d cur_len=%d\n", sizeof(struct sess)+pt->se.len, len);
	if(len >=16 && pt->se.flag == S_HEART_BEAT)
	{
		DEBUG("a heart beat!\n");
		CHECK2(pt->se.len == 11);
		for(i = 0; i < pt->se.len; i++)
			CHECK2(pt->se.data[i] == 0xff);
		memmove(buf, buf+16, len - 16);
		offset = len - 16;
		return 0;

	}
	int data_len = pt->de.len + sizeof(struct pres_task);
	if(data_len > len)
	{
		DEBUG("data_len:%d real_len:%d\n", data_len, len);
		return -1;
	}

		printf("session: data_len:%d flag:%d\n", pt->se.len, pt->se.flag);
		printf("present: src_code:%x dst_code:%x len:%d\n", 
			pt->pr.src_tel_code, pt->pr.dst_tel_code, pt->pr.len);
		printf("detail:time:%s type:%x sub_type:%x speaker:%x connector:%x len:%d\n",
			ctime(&pt->de.time), pt->de.type, pt->de.sub_type, pt->de.speaker, pt->de.connector, pt->de.len);
	if(pt->de.type == D_TYPE_TEXT)
		show_binary(pt->de.data, pt->de.len);
	else
	{
		int fd = open("rcv.dat", O_CREAT|O_WRONLY|O_TRUNC, 0666);
		/*for(i = 0; i < pt->de.len; i++)
			printf("%c", pt->de.data[i]);
		printf("\n");
		*/
		CHECK2(write(fd, pt->de.data, pt->de.len));
		close(fd);
		//exit(0);
	}
	memmove(buf, buf+data_len, len - data_len);
	offset = len - data_len;
	return 0;	
}



char text[TASK_LEN];
char voice[TASK_LEN];
struct pres_task *tsk_text = (struct pres_task*)text;
struct pres_task *tsk_voice = (struct pres_task*)voice;
void inc_task()
{
	int i;
	int len = (rand()%(2000-100))+1;
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
	tsk_voice->de.type = D_TYPE_VOICE;
}
#define TEST 30000000
int main5()
{
	start_tcp_client();
	int cnt = TEST;
	while(cnt--)
	{
		INFO("\033[1;33mspd client run... main5\033[0m \n");
		DEBUG("npkt_send:%d npkt_recv:%d\n", npkt_send, npkt_recv);
		if(!is_connect())
			sleep(1);
		//sleep(1);
		while(have_pkt())
		{
			int len = get_frame(rcv_buf, TASK_LEN);
			if(len < 0)
			{
				DEBUG("get frame error!\n");
			}
			else
			{
				DEBUG("get a frame!!\n");
				CHECK2(!show(rcv_buf, len));
				npkt_recv ++;
			}
		}
		if(!have_pkt())
			DEBUG("NO packet ......\n");
		if(!is_busy())
			inc_task();
		if(!is_busy())
		{
			DEBUG("last task status:%d\n", get_status());
			submit_task((char*)tsk_text, tsk_text->se.len + sizeof(struct sess), NULL);
			npkt_send++;
		}
		else
		{
			DEBUG("SUBMIT text failed!\n");
		}
		usleep(100000);
		if(!is_busy())
		{
			printf("last task status:%d\n", get_status());
			submit_task((char*)tsk_voice, sizeof(struct pres_task), "JE.txt");
			npkt_send++;
			DEBUG("-------task submited:%d\n", npkt_send);
		}
		else
		{
			DEBUG("SUBMIT voice failed\n");
		}
	//	usleep(1000000);
	}
	stop_tcp_client();
	return 0;
}
