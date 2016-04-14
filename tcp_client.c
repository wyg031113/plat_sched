#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "plat_sched.h"
#include "debug.h"
int serfd;
int connect_server(void)
{
	struct sockaddr_in app_addr;
	CHECK(serfd = socket(AF_INET, SOCK_STREAM, 0));
	bzero(&app_addr, sizeof(app_addr));
	app_addr.sin_family = AF_INET;
	app_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	app_addr.sin_port = htons(8888);
	CHECK(connect(serfd, (struct sockaddr*)&app_addr, 
							sizeof(struct sockaddr)));
	return serfd;
}
#define BUF_SIZE 2048
char buf[BUF_SIZE];
struct sched_pres *sp = (struct sched_pres*)buf;
void inc_sp(void)
{
	int i, text_len;
	sp->src_tel_code++;
	sp->dst_tel_code++;
	sp->no++;
	sp->time = time(NULL);
	sp->data_type++;
	sp->data_sub_type++;
	text_len = rand()%(BUF_SIZE-23)+1;
	for(i = 0; i < text_len; i++)
		sp->data[i] = i;
	sp->pkt_len = 23 + text_len;
	sp->text_len = text_len;
}
int main()
{
	connect_server();
	int cnt = 10000;
	int last_success = 1;
	int send_offset = 0;
	int ret;
	sleep(2);
	while(cnt--)
	{
		if(last_success)
			inc_sp();
		CHECK(ret = send(serfd, (char*)sp+send_offset, sp->pkt_len-send_offset, 0));
		send_offset += ret;
		if(send_offset == sp->pkt_len)
		{
			DEBUG("Send no:%d len=%d\n", sp->no, send_offset);
			send_offset = 0;
			last_success = 1;
		}
		else
		{
			last_success = 0;
		}
		
		DEBUG("Run cnt=%d ...\n", cnt);
		//usleep(10000);
	}
	return 0;
}
