#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "plat_sched.h"
#include "debug.h"

#define DEFAULT_TIME_OUT 100000				//recv的超时时间，单位:毫秒
#define MAX_PKT_LEN 1024					//单个包最大长度

/*注意，环形缓冲区大小必须是2^n大小*/
#define RCV_CB_BUF_SIZE 4096
#define SND_CB_BUF_SIZE 4096

/*接收和发送缓冲区定义
 */
static char rcv_cb_buf[RCV_CB_BUF_SIZE];
static char snd_cb_buf[SND_CB_BUF_SIZE];
static struct circle_buffer cb_rcv;
static struct circle_buffer cb_snd;

static volatile int web_server_start = 0;	
static volatile time_t last_heart_beat = 0;
/*临时发送接受缓冲区
 * 首先把数据从环形缓冲区复制到临时缓冲区中，再发送
 * 首先把数据接收到临时缓冲区中，再放到环形缓冲区中
 */
static char rcv_buf[MAX_PKT_LEN];
static char snd_buf[MAX_PKT_LEN];


void check_addto_rcvbuf(char *data, int len);
void send_packet(void);
void *sig_voice_rcv_thread(void *arg);			//接收主线程
void *sig_voice_snd_thread(void *arg);			//发送主线程

static volatile int stop_sv = 0;			//是否停止主线程 是=1  否=0

static char webip[IPADDR_LEN]="127.0.0.1";	//WEB端UDP服务器IP
static unsigned short webport = 7777;		//WEB端UDP服务器端口

static char preip[IPADDR_LEN]="0.0.0.0";	//区长台UDP服务器监听IP
static unsigned short preport = 6666;		//区长台UDP服务器监听端口


static struct sockaddr_in webaddr;			
socklen_t webaddr_len;
static int websockfd;						//web端的socket

/*判断web端是否启动
 * return 1-启动 0－未启动
 */
int is_web_server_started(void)
{
	return web_server_start;
}

/*设置WEB端服务器IP和端口
 * ipaddr: WEB端服务器IP
 * p:	   WEB端服务器端口
 */
void set_webip_port(const char *ipaddr, unsigned short p)
{
	strncpy(webip, ipaddr, IPADDR_LEN);
	webport = p;
	DEBUG("set webip:%s port:%d\n", webip, webport);
}

/*设置区长台服务器IP和端口
 * ipaddr: 区长台服务器IP
 * p:	   区长台端服务器端口
 */
void set_preip_port(const char *ipaddr, unsigned short p)
{
	strncpy(preip, ipaddr, IPADDR_LEN);
	preport = p;
	DEBUG("set preip:%s port:%d\n", preip, preport);
}

/*启动sig voice 主线程
 */
pthread_t sig_voice_rcv_tid;
pthread_t sig_voice_snd_tid;
void start_sig_voice(void)
{
	int ret = 0;

	cirbuf_init(&cb_rcv, rcv_cb_buf, RCV_CB_BUF_SIZE);
	cirbuf_init(&cb_snd, snd_cb_buf, SND_CB_BUF_SIZE);
	CHECK2( (ret = pthread_create(&sig_voice_rcv_tid, NULL, sig_voice_rcv_thread, NULL)) == 0);
	CHECK2( (ret = pthread_create(&sig_voice_snd_tid, NULL, sig_voice_snd_thread, NULL)) == 0);
	INFO("sig voice started!\n");
}

/*停止主线程
 */
void stop_sig_voice(void)
{
	stop_sv = 1;
	pthread_join(sig_voice_rcv_tid, NULL);
	pthread_join(sig_voice_snd_tid, NULL);
}

void handle_sig_voice(void);

/**线程函数
 */
void *sig_voice_rcv_thread(void *arg)
{
	DEBUG("voice sig thread started!\n");
	handle_sig_voice();
}

void prepare_webaddr(void);
void *sig_voice_snd_thread(void *arg)
{
	prepare_webaddr();
	while(!stop_sv)
	{
		if(!cirbuf_empty(&cb_snd))
			send_packet();
		else
			usleep(300000);
	}
	return NULL;
}
/*设置socfd非阻塞
 */
void setnonblocking(int sockfd)
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
	while(!stop_sv)
	{
		ret = recvfrom(serfd, rcv_buf, MAX_PKT_LEN, 0,
				(struct sockaddr*)&webaddr, &webaddr_len);
		if(ret<=0)
		{
			if(errno == EAGAIN)
			{
				//DEBUG("Recv timed out!\n");
				continue;
			}
			else if(errno == EINTR)
			{
				DEBUG("Interruped!\n");
			}
			else
				CHECK(ret);
		}
		else
		{
			//DEBUG("Recv a udp packet!\n");
			check_addto_rcvbuf(rcv_buf, ret);
		}

		if(time(NULL) - last_heart_beat > WEB_HEART_BEAT_TIMEDOUT)
			web_server_start = 0;
		else
			web_server_start = 1;
	}
	INFO("Sig recv stopped!\n");
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

		if(cs->data == CS_DATA_HEART_BEAT)
		{
			last_heart_beat = time(NULL);	
		}
		real = copy_cirbuf_from_user(&cb_rcv, data, len);
		CHECK2(real == len);
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
		CHECK2(real == len);
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
		DEBUG("SEND FAIL!\n");
		if(errno == EAGAIN)
			return 0;
		else
			CHECK(ret);
	}
	return 1;
}

/*将环形缓冲区中的包发出去
 */
void send_packet(void)
{
	int ret;

	//注意这两个静态变量,函数退出后依然保持
	static int len = 0;
	static int beSended = 1;
	//首先检查上次发送结果，如果失败就重新发送
	if(!beSended)
	{
		DEBUG("Resend\n");
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
	else if(snd_buf[1] == V_CMD) //语音
	{
		struct voice *vc = (struct voice*)snd_buf;
		ret = copy_cirbuf_to_user(&cb_snd, snd_buf+2, 4);
		CHECK2(ret == 4 &&  vc->len+6 <= MAX_PKT_LEN);
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

/*将一个信令包放到环形缓冲区中
 * 返回：EBUFF_FULL缓形缓冲区満，一个字节也没有放入
 *       PS_SUCCESS放入成功
 */
int put_sig(struct control_sig *cs)
{
	int real_len = sizeof(struct control_sig);
	if(real_len > cirbuf_get_free(&cb_snd))
		return EBUFF_FULL;

	ASSERT(cs->type == VC_TYPE && cs->cmd == CS_CMD);
	CHECK2(copy_cirbuf_from_user(&cb_snd, (uint8 *)cs, real_len) == real_len);
	DEBUG("put sig success\n");
	return PS_SUCCESS;
}

/*放入一个语音包到环形缓冲区中 
 * 返回：EBUFF_FULL缓形缓冲区満，一个字节也没有放入
 *       PS_SUCCESS放入成功
 */
int put_voice(struct voice *vc)
{
	int real_len = 0;
	ASSERT(vc->type == VC_TYPE && vc->cmd == V_CMD);
	real_len = sizeof(struct voice) + vc->len;
	if(real_len > cirbuf_get_free(&cb_snd))
		return EBUFF_FULL;
	CHECK2(copy_cirbuf_from_user(&cb_snd, (uint8 *)vc, real_len) == real_len);
	DEBUG("put voice success\n");
	return PS_SUCCESS;
}

/*获取缓冲区头的包类型，并且可以判断缓冲区中有没有包。
 * return: MSG_NOMSG 没有包
 *         MSG_SIGNAL 信令包
 *         MSG_VOICE 语音包
 */
int get_msg_type(void)
{
	struct control_sig cs;
	if(cirbuf_empty(&cb_rcv))
		return MSG_NOMSG;
	CHECK2(copy_cirbuf_to_user_flag(&cb_rcv, (uint8 *)&cs, 2, COPY_ONLY) == 2);
	ASSERT(cs.type == VC_TYPE);
	ASSERT(cs.cmd == CS_CMD || cs.cmd == V_CMD);
	if(cs.cmd == CS_CMD)
		return MSG_SIGNAL;
	else
		return MSG_VOICE;
}

/*从环形缓冲区获取一个语音包
 * return: EUSER_BUFF_TOO_SHORT 用户缓冲区太短，放不下一个包
 *		   PS_SUCCESS:获取成功
 */
int get_voice(struct voice *vc, uint32 len)
{
	if(len < 6)
		return EUSER_BUFF_TOO_SHORT;
	ASSERT(vc != NULL);
	CHECK2(copy_cirbuf_to_user_flag(&cb_rcv, (uint8 *)vc, 6, 0) == 6);
	if(len < vc->len + 6)
		return EUSER_BUFF_TOO_SHORT;
	CHECK2(copy_cirbuf_to_user(&cb_rcv, (uint8 *)vc, vc->len+6) == vc->len + 6);
	return PS_SUCCESS;
}

/*从环形缓冲区获取一个信令包
 * return: EUSER_BUFF_TOO_SHORT 用户缓冲区太短，放不下一个包
 *		   PS_SUCCESS获取成功
 */
int get_sig(struct control_sig *cs)
{
	int len = sizeof(struct control_sig);
	ASSERT(cs != NULL);
	CHECK2(copy_cirbuf_to_user(&cb_rcv, (uint8 *)cs, len) == len);
	return PS_SUCCESS;
}
