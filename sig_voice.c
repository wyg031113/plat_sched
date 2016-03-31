#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

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
void send_packet(void);
void *sig_voice_thread(void *arg);			//主线程

static volatile int stop_sv = 0;			//是否停止主线程 是=1  否=0

static char webip[16]="127.0.0.1";			//WEB端UDP服务器IP
static unsigned short webport = 7777;		//WEB端UDP服务器端口

static char preip[16]="0.0.0.0";			//区长台UDP服务器监听IP
static unsigned short preport = 6666;		//区长台UDP服务器监听端口

static struct sockaddr_in webaddr;
static int websockfd;
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

/*设置socfd非阻塞
 */
static void setnonblocking(int sockfd)
{
	int opts;

	CHECK(opts = fcntl(sockfd, F_GETFL));
	opts = opts | O_NONBLOCK;
	CHECK(fcntl(sockfd, F_SETFL, opts));
}

/* 准备好web 端socket和地址
 * 发送数据到web
 */
void prepare_webaddr(void)
{
	int val = 1;
	CHECK(websockfd = socket(AF_INET, SOCK_DGRAM, 0));
	bzero(&webaddr, sizeof(webaddr));
	webaddr.sin_family = AF_INET;
	webaddr.sin_addr.s_addr = inet_addr(webip);
	webaddr.sin_port = htons(webport);
	CHECK(setsockopt(websockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
	setnonblocking(websockfd);
}
/*主要功能都在这里
 */
void handle_sig_voice(void)
{
	int serfd;
	int ret;
	serfd = start_server();
	prepare_webaddr();
	while(!stop_sv)
	{
		DEBUG("Main loop.\n");
		send_packet();
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
	close(websockfd);
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
	
	//创建地址
	bzero(&seraddr, sizeof(seraddr));
	seraddr.sin_family = AF_INET;
	seraddr.sin_addr.s_addr = inet_addr(preip);
	seraddr.sin_port = htons(preport);

	//设置地址重用
	CHECK(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));

	//设置超时
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

/* 发送UPD包
 * buf:缓冲区
 * len:要发送的长度
 * return: 1-发送成功 0-发送失败
 */
int udp_snd(uint8 *buf, int len)
{
	int ret = sendto(websockfd, buf, len, 0, (struct sockaddr*)&webaddr, sizeof(struct sockaddr));
	if(ret <=0 )
	{
		if(errno == EAGAIN)
			return 0;
		else
			CHECK(ret);
	}
	return 1;
}
void send_packet(void)
{
	int ret;

	//注意这两个静态变量,函数退出后依然保持
	static int len = 0;
	static int beSended = 1;
	//首先检查上次发送结果，如果失败就重写发送
	if(!beSended)
	{
		beSended = udp_snd(snd_buf, len);
		return;
	}
	if(cirbuf_empty(&cb_snd))
		return;

	//从环形缓冲区里复制出一个包，要区分是信令还是声音
	//这里默认相信环形缓冲里的包是完好的。ASSERT在取消DEBUG后会失效
	ret = copy_cirbuf_to_user(&cb_snd, snd_buf, 2);
	ASSERT(ret == 2 && snd_buf[0] == VC_TYPE);
	if(snd_buf[1] == CS_CMD) //信令
	{
		ret = copy_cirbuf_to_user(&cb_snd, snd_buf+2, sizeof(struct control_sig)-2);
		ASSERT(ret == (sizeof(struct control_sig)-2));
		len = sizeof(struct control_sig);
	}
	else if(snd_buf[1] == V_CMD) //声音
	{
		struct voice *vc = (struct voice*)snd_buf;
		ret = copy_cirbuf_to_user(&cb_snd, snd_buf+2, 4);
		ASSERT(ret == 4);
		ret = copy_cirbuf_to_user(&cb_snd, vc->data, vc->len);
		ASSERT(ret == vc->len);
		len = vc->len + sizeof(struct voice);
	}
	beSended = udp_snd(snd_buf, len);
	return;
}

////////////////////////////////////////////////////////////////////
//		对外接口定义											  //
//----------------------------------------------------------------//		
