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

int main()
{
	int cnt = 1000000;
	start_pres_server();
	struct sched_pres *sp = (struct sched_pres*)buf;
	uint8 no = 0;
	while(1)
	{
		if(!is_client_connected() && !have_packet())
		{
			no = 0;
			usleep(1000000);
		}
		if(have_packet())
		{
			int len = get_packet(buf, MAX_LEN);
			CHECK2(len >=23 && len == sp->pkt_len);
			INFO("Get a packet!\n dst_tel_code:%x\nsrc_tel_code:%x\n"
				 "pkt_len:%d\nno%d\ntime:%x\ndata_type:%x\ndata_sub_type:%x\n"
				 "test_len:%d\n",
				 sp->dst_tel_code, sp->src_tel_code, sp->pkt_len, sp->no, sp->time,
				 sp->data_type, sp->data_sub_type, sp->text_len);
			int i;
			uint8 c = 0;
			no++;
			CHECK2(no==sp->no);
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
		else
			usleep(100000);
	}
	stop_pres_server();
}
