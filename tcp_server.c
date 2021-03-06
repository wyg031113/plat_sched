/*************************************************************************
    > File Name: tcp_server.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2016年04月10日 星期日 09时54分13秒
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "plat_sched.h"
#include "debug.h"
#define MAX_LEN 2048
char buf[MAX_LEN];

int main4()
{
	int cnt = 1000000;
	start_pres_server();
	struct sched_pres *sp = (struct sched_pres*)buf;
	uint8 no = 0;
		
	while(1)
	{
		INFO("\033[1;33mtcp server... main4\033[0m \n");
		if(!is_client_connected() && !have_packet())
		{
			no = 0;
			DEBUG("no client!\n");
			usleep(1000000);
		}
		if(have_packet())
		{
			int len = get_packet(buf, MAX_LEN);
			CHECK2(len >=23 && len == sp->pkt_len);
			INFO("Get a packet!\n dst_tel_code:%x\nsrc_tel_code:%x\n"
				 "pkt_len:%d\nno%d\ntime:%s\ndata_type:%x\ndata_sub_type:%x\n"
				 "test_len:%d\n",
				 sp->dst_tel_code, sp->src_tel_code, sp->pkt_len, sp->no, ctime(&sp->time),
				 sp->data_type, sp->data_sub_type, sp->text_len);
			int i;
			uint8 c = 0;
			no++;
		//	DEBUG("should no:%d  RECV_NO:%d\n", no, sp->no);
	//		CHECK2(no==sp->no);
			for(i = 0; i < sp->text_len; i++)
			{
				if(sp->data[i] != c)
				{
					DEBUG("i = %d sp->data[i]==%d  c==%d\n", i, sp->data[i], c);
					CHECK2(sp->data[i]==c);
				}
				c++;
			}
		}
			usleep(10000);
	}
	stop_pres_server();
}
