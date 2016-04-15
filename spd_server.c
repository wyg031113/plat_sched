#include <stdio.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "debug.h"
#include "plat_sched.h"

#define TASK_LEN 2004800

int npkt_send, npkt_recv;

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

time_t last_send_heart;
char heart_beat_packet[16] = { 0xb, 0x00, 0x00, 0x00, 0x0f, 0xFF, 0xFF, 0xFF, 0XFF, 0xFF,0xFF, 0xFF, 0xFF,0xFF, 0xFF,0xFF};
/*开始监听
 */
static int start_listen(void)
{
	int sockfd;
	struct sockaddr_in ser_addr;
	int val = 1;
	CHECK(sockfd = socket(AF_INET, SOCK_STREAM, 0));

	bzero(&ser_addr, sizeof(ser_addr));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	ser_addr.sin_port = htons(5555);

	CHECK(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
	CHECK(bind(sockfd, (struct sockaddr*)&ser_addr, sizeof(struct sockaddr)));
//	setnonblocking(pres_serfd); //设置非阻塞
	CHECK(listen(sockfd, 5));
	return sockfd;
}
char rcv_buf[TASK_LEN];
int offset = 0;
void show_binary(char *buf, int len)
{

	//---------------------
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
void show(char *buf, int len)
{
	int i;
	struct pres_task *pt = (struct pres_task*)buf;
	//if(len >= sizeof(struct sess))
	//	INFO("ptk len = %d cur_len:%d\n", pt->se.len + sizeof(struct sess), len);
	if(len >=16 && pt->se.flag == S_HEART_BEAT)
	{
		DEBUG("a heart beat!\n");
		CHECK2(pt->se.len == 11);
		for(i = 0; i < pt->se.len; i++)
			CHECK2(pt->se.data[i] == 0xff);
		memmove(buf, buf+16, len - 16);
		offset = len - 16;
		return;

	}
	int data_len = pt->de.len + sizeof(struct pres_task);
	if(data_len > len)
	{
		//DEBUG("data_len:%d real_len:%d\n", data_len, len);
		return;
	}

		printf("session: data_len:%d flag:%d\n", pt->se.len, pt->se.flag);
		printf("present: src_code:%x dst_code:%x len:%d\n", 
			pt->pr.src_tel_code, pt->pr.dst_tel_code, pt->pr.len);
		printf("detail:time:%s type:%x sub_type:%x speaker:%x connector:%x len:%d\n",
			ctime((time_t*)&pt->de.time), pt->de.type, pt->de.sub_type, pt->de.speaker, pt->de.connector, pt->de.len);
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
	
}

void *snd_thread(void *arg)
{
	int client_fd = (int)arg;
	int ret;
	while(1)
	{
		INFO("\033[1;33mspd server snd thread run... main6\033[0m \n");
		if(time(NULL) - last_send_heart > 1)
		{

			if(send(client_fd, heart_beat_packet, sizeof(heart_beat_packet), 0) != sizeof(heart_beat_packet))
			{
				DEBUG("send heart beat failed!\n");
				break;
			}
			DEBUG("Send a heart beat packet!\n");
			last_send_heart = time(NULL);
		}

		inc_task();
		int len = tsk_text->se.len + sizeof(struct sess);
		while(len>0)
		{
			if( (ret = send(client_fd, text, len, 0)) <= 0)
			{
				close(client_fd);

				return NULL;
			}
			else
			{

				len -= ret;
				DEBUG("SEND %d bytes\n", ret);
			}
			
		}
			npkt_send++;
			DEBUG("Send a text packet!\n");
			usleep(100000);
	}
	close(client_fd);
}
void *rcv_thread(void *arg)
{
	pthread_detach(pthread_self());
	int client_fd = (int)arg;
	int i;
	while(1)
	{
		INFO("\033[1;33mspd server rcv thread run... main6\033[0m \n");
		int ret = recv(client_fd, rcv_buf+offset, TASK_LEN-offset, 0);
		//DEBUG("rcv awake!\n");

		if(ret<=0)
		{
			close(client_fd);
			break;
		}
		npkt_recv++;
		offset += ret;
		for(i = 0; i < 10; i++)
			show(rcv_buf, offset);
	//	show_binary(rcv_buf, ret);
	//	offset = 0;
		//sleep(1);
	}
	close(client_fd);
}
int main6()
{
	int ser_fd = start_listen();
	int client_fd;
	pthread_t tid;
	while(1)
	{
		client_fd = accept(ser_fd, NULL, NULL);
		if(client_fd < 0)
			break;
		else
		{
			DEBUG("a client!\n");
			pthread_create(&tid, NULL, rcv_thread, (void*)client_fd);
			pthread_create(&tid, NULL, snd_thread, (void*)client_fd);
		}
	}
	return 0;
}
