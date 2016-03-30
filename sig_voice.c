#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "plat_sched.h"
#include "debug.h"

#define DEFAULT_TIME_OUT 100000			//recv的超时时间，单位:微秒
#define MAX_PKT_LEN 1024
#define RCV_CB_BUF_SIZE 2048
#define SND_CB_BUF_SIZE 2048

/*接收和发送缓冲区定义
 */
static char rcv_cb_buf[RCV_CB_BUF_SIZE];
static char snd_cb_buf[SND_CB_BUF_SIZE];
static struct circle_buffer cb_rcv;
static struct circle_buffer cb_snd;

/*临时发送接受缓冲区
 */
static char rcv_buf[MAX_PKT_LEN];
static char snd_buf[MAX_PKT_LEN];

void check_addto_rcvbuf(char *data, int len);
void *sig_voice_thread(void *arg);			//主线程

static volatile int stop_sv = 0;			//是否停止主线程 是=1  否=0

static char webip[16]="127.0.0.1";			//WEB端UDP服务器IP
static unsigned short webport = 7777;		//WEB端UDP服务器端口

static char preip[16]="0.0.0.0";			//区长台UDP服务器监听IP
static unsigned short preport = 6666;		//区长台UDP服务器监听端口

/*设置WEB端服务器IP和端口
 * ipaddr: WEB端服务器IP
 * p:	   WEB端服务器端口
 */
void set_webip_port(const char *ipaddr, unsigned short p)
{
	strncpy(webip, ipaddr, 16);
	webport = p;
	DEBUG("set webip:%s port:%d\n", webip, webport);
}

/*设置区长台服务器IP和端口
 * ipaddr: 区长台服务器IP
 * p:	   区长台端服务器端口
 */
void set_preip_port(const char *ipaddr, unsigned short p)
{
	strncpy(preip, ipaddr, 16);
	preport = p;
	DEBUG("set preip:%s port:%d\n", preip, preport);
}

/*启动sig voice 主线程
 */
void start_sig_voice(void)
{
	pthread_t tid;
	int ret = 0;

	cirbuf_init(&cb_rcv, rcv_cb_buf, RCV_CB_BUF_SIZE);
	cirbuf_init(&cb_snd, snd_cb_buf, SND_CB_BUF_SIZE);
	CHECK2( (ret = pthread_create(&tid, NULL, sig_voice_thread, NULL)) == 0);
}

/*停止主线程
 */
void stop_sig_voice(void)
{
	stop_sv = 1;
}

void handle_sig_voice(void);

/**线程函数
 */
void *sig_voice_thread(void *arg)
{
	pthread_detach(pthread_self());
	DEBUG("voice sig thread started!\n");
	handle_sig_voice();
}

/*主要功能都在这里
 */
void handle_sig_voice(void)
{
	int serfd = start_server();
	int ret;
	while(!stop_sv)
	{
		DEBUG("Main loop.\n");
		ret = recvfrom(serfd, rcv_buf, MAX_PKT_LEN, 0, NULL, NULL);
		if(ret<=0)
		{
			if(errno == EAGAIN)
			{
				DEBUG("Recv timed out!\n");
				continue;
			}
			else
				CHECK(ret);
		}
		else
		{
			DEBUG("Recv a udp packet!\n");
			check_addto_rcvbuf(rcv_buf, ret);
		}
	}
	DEBUG("Main loop stopped!\n");
	close(serfd);
}

/*创建UDP socket，并且绑定地址
 */
int start_server(void)
{
	int sockfd;
	int val = 1;
	struct sockaddr_in seraddr;
	struct timeval tv;
	CHECK(sockfd = socket(AF_INET, SOCK_DGRAM, 0));

	bzero(&seraddr, sizeof(seraddr));
	seraddr.sin_family = AF_INET;
	seraddr.sin_addr.s_addr = inet_addr(preip);
	seraddr.sin_port = htons(preport);

	CHECK(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));

	tv.tv_sec = DEFAULT_TIME_OUT / 1000000;
	tv.tv_usec = DEFAULT_TIME_OUT % 1000000;
	CHECK(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
	CHECK(bind(sockfd, (struct sockaddr*)&seraddr, sizeof(seraddr)));
	return sockfd;
}

/*检测数据包格式和内容
 * 这里有很多数字。
 */
void check_addto_rcvbuf(char *data, int len)
{
	struct control_sig *cs = (struct control_sig *)data;
	if(len < 2)//包长度太短
	{
		INFO("Recv a bad len packet, len<2!!\n");
		return;
	}
	if(cs->type != VC_TYPE) //包类型不正确,第一个字节必须是0x02
	{
		INFO("Recv a bad type packet, just drop it!\n");
		return;
	}

	if(cs->cmd == CS_CMD) //是信令
	{
		int real = 0;
		if(len != sizeof(struct control_sig))
		{
			INFO("Recv a bad sig packet, size not right!\n");
			return;
		}
		if( cirbuf_get_free(&cb_rcv) < len )
		{
			INFO("Recv buffer full, just drop packet!\n");
			return;
		}
		real = copy_cirbuf_from_user(&cb_rcv, data, len);
		CHECK2((real != len));
	}
	else if(cs->cmd == V_CMD) //是语音
	{
		int real = 0;
		struct voice *vc = (struct voice*)data;
		if(len < 6)
		{
			INFO("Recv bad voice packet, len < 8\n");
			return;
		}
		if(vc->len != len - 6)
		{
			INFO("Recv bad voice len packet, vc->len=%d, packet_len - 6 = %d\n", vc->len, len - 6);
			return;
		}
		if(cirbuf_get_free(&cb_rcv) < len )
		{
			INFO("Recv buffer full, just drop packet!\n");
			return;
		}
		real = copy_cirbuf_from_user(&cb_rcv, data, len);
		CHECK2((real != len));
	}
	else
	{
		INFO("Recv a bad cmd type packet, just drop it!\n");
		return;
	}
}
