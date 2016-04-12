#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "plat_sched.h"

/*开始监听
 */
int start_listen(void)
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
#define TASK_LEN 2048
char rcv_buf[TASK_LEN];
int offset = 0;
int flag = 0;
int data_len = 0;
void show(char *buf, int len)
{
	int i;
	struct pres_task *pt = (struct pres_task*)buf;
	if(flag == 0)
	{

		printf("session: data_len:%d flag:%d\n", pt->se.len, pt->se.flag);
		printf("present: src_code:%x dst_code:%x len:%d\n", 
			pt->pr.src_tel_code, pt->pr.dst_tel_code, pt->pr.len);
		printf("detail:time:%x type:%x sub_type:%x speaker:%x connector:%x len:%d\n",
			pt->de.time, pt->de.type, pt->de.sub_type, pt->de.speaker, pt->de.connector, pt->de.len);
		data_len = pt->de.len;
		int rcv_data_len = len - sizeof(struct pres_task);
		int print_len=0;
		if(rcv_data_len < data_len)
		{
			flag = 1;
			print_len = rcv_data_len;
			data_len -= rcv_data_len;
			offset = 0;
		}
		else
		{
			flag = 0;
			print_len = data_len;
			data_len = 0;
		}
		for(i = 0; i < print_len; i++)
			printf("%c", pt->de.data[i]);
		int x = sizeof(struct pres_task)+print_len;
		memmove(buf, buf+x, len-x);
		offset = len-x;
	}
	else
	{
		int print_len = data_len < len ? data_len : len;
		for(i = 0; i < print_len; i++)
			printf("%c", pt->de.data[i]);
		data_len -= print_len;
		if(data_len ==0)
		{
			printf("\n");	
			flag = 0;
		}
		int x = print_len;
		memmove(buf, buf+x, len-x);
		offset = len-x;

	}
}
void *rcv_thread(void *arg)
{
	pthread_detach(pthread_self());
	int client_fd = (int)arg;
	int i;
	while(1)
	{
		int ret = recv(client_fd, rcv_buf+offset, TASK_LEN-offset, 0);
		if(ret<=0)
		{
			close(client_fd);
			break;
		}
		show(rcv_buf, ret);
	}
}
int main()
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
		}
	}
	return 0;
}
