#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "debug.h"
#include "plat_sched.h"
#define MAX_SEG_SIZE 2048  //临时接收缓冲区大小
#define RECV_TIME_OUT 1000 //接收数据超时时间
#define SLEEP_TIME 500000  //无客户端时睡眠时间

//临时接收缓冲区，收到完成的一个包后会加入到环形缓冲区
static char tmp_rcv_buf[MAX_SEG_SIZE];	

//监听ip和端口
static char pres_ser_ip[IPADDR_LEN] = "127.0.0.1";
static unsigned short pres_ser_port  = 8888;

//监听socket
static int pres_serfd;
//客户端socket
//static int pres_clientfd;
//是否停止服务器
static volatile int stop_server = 0;
//线程tid
pthread_t pres_server_tid;
//有没有客户端边接
static volatile int have_client = 0;
static volatile int stop_client = 0;
//注意，环形缓冲区大小必须是2^n大小
#define RCV_CB_SER_BUF_SIZE 4096*32
/*接收缓冲区定义
 */
static char rcv_cb_buf_ser[RCV_CB_SER_BUF_SIZE];
static struct circle_buffer cb_rcv_ser;

void *pres_server_thread(void *arg);	 //线程函数
void pres_server_run(void);				 //主循环
int check_add_cb(char *buf, int len);	 //把收到的数据加入到环形缓冲区中



/*开始监听
 */
int start_listen(void)
{
	struct sockaddr_in ser_addr;
	int val = 1;
	CHECK(pres_serfd = socket(AF_INET, SOCK_STREAM, 0));

	bzero(&ser_addr, sizeof(ser_addr));
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(pres_ser_ip);
	ser_addr.sin_port = htons(pres_ser_port);

	CHECK(setsockopt(pres_serfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));
	CHECK(bind(pres_serfd, (struct sockaddr*)&ser_addr, sizeof(struct sockaddr)));
	setnonblocking(pres_serfd); //设置非阻塞
	CHECK(listen(pres_serfd, 5));
	return pres_serfd;
}

/*初始化，起动主线程
 */
void start_pres_server(void)
{
	cirbuf_init(&cb_rcv_ser, rcv_cb_buf_ser, RCV_CB_SER_BUF_SIZE);
	CHECK2(pthread_create(&pres_server_tid, NULL, pres_server_thread, NULL)==0);
}

/*停止主线程
 */
void stop_pres_server(void)
{
	stop_server = 1;
	pthread_join(pres_server_tid, NULL);
}

void *pres_server_thread(void *arg)
{
	DEBUG("server thread started!\n");
	pres_server_run();
	return NULL;
}

/*收数据从应用服务器。
 * 接收够len字节才返回，除非要停止
 * return 实际接收字节数
 */
int rcv_data(int fd, void *data, int len)
{
	int offset = 0;
	int ret;
	while(offset < len && !stop_server && !stop_client)
	{
		ret = recv(fd, data, len, 0);
		if((ret == -1 && errno != EAGAIN) || ret == 0)
		{
			//raise(SIGPIPE);
			break;
		}
		else if(ret == -1 && errno == EAGAIN)
		{
			//DEBUG("RCV_TIMED_OUT:cur time:%d  last time:%d\n", time(NULL), last_rcv_heart);
			/*if(time(NULL) - last_rcv_heart > WEB_HEART_BEAT_TIMEDOUT)
			{

				//raise(SIGPIPE);
				DEBUG("no heart beat.....\n");
				break;
			}
			*/
			DEBUG("Timed out!\n");

		}
		else
		{
			offset += ret;
			//last_rcv_heart = time(NULL);
		}
	}
	return offset;
}
int handle_client(int client_fd)
{

	static int len = 0;
	int ret;
	struct sched_pres *sp = (struct sched_pres *)tmp_rcv_buf;
	static int last_success = 1;
	int ret_status = PS_SUCCESS;
	if(last_success)
	{	
		
		//接收数据包头部
		len = 0;
		if((ret = rcv_data(client_fd, tmp_rcv_buf, sizeof(struct sched_pres))) != sizeof(struct sched_pres))
		{
			return  PS_RECV_ERROR;
		}


		len += ret;	
		DEBUG("RECV header:%d bytes, text len:%d bytes\n", ret, sp->text_len);
		if(sp->text_len + sizeof(struct sched_pres) > MAX_SEG_SIZE)
		{
			INFO("tcp frame longer than tmp buffer!\n");
			return  PS_RECV_ERROR;
		}
		
		if((ret = rcv_data(client_fd, tmp_rcv_buf+len, sp->text_len)) != sp->text_len)
		{
			DEBUG("tcp recv data failed!\n");
			return PS_RECV_ERROR;
		}

		len += ret;
		last_success = 0;
	}

	if(!last_success)
	{
		if(cirbuf_get_free(&cb_rcv_ser) < len)
		{
			usleep(100000);
			return PS_SUCCESS;
		}
		CHECK2(copy_cirbuf_from_user(&cb_rcv_ser, tmp_rcv_buf, len) == len);
		DEBUG("PUT to circle %d bytes\n", len);
		last_success = 1;
	}
	DEBUG("handle client stoped!");
	return PS_SUCCESS;;
}
/*
 */
void pres_server_run(void)
{
	int client_fd = -1;	
	int tfd = -1;
	struct timeval tv;;
	int last_success = 1; 
	int len = 0;
	int ret = 0;
	pthread_t tid;
	CHECK(start_listen()); 
	while(!stop_server)
	{
		DEBUG("run.., len = %d\n", len);
		tfd = accept(pres_serfd, NULL, NULL); //接收客户端连接，由于pers_serfd设置为非阻塞，会立即返回
		if(tfd == -1 && errno == EAGAIN)
		{
			DEBUG("ACCEPT timed out!\n");
			if(client_fd < 0)
				usleep(500000);
			else
			{
				DEBUG("Handle client!\n");
				if(handle_client(client_fd) != PS_SUCCESS)
				{
					close(client_fd);
					client_fd = -1;
				}

			}
			continue;
		}
		CHECK(tfd);	
		if(client_fd > 0)				//由于只有一个客户端，所有断定上个连接已经终止，关闭socket
		{
			DEBUG("NEW CLIENT!\n");
			close(client_fd);
			client_fd = -1;
			DEBUG("OLD THREAD STOPED!!!");
		}

		DEBUG("accept a new client\n");
		client_fd = tfd;
		have_client = 1;
		//设置超时
		tv.tv_sec = RECV_TIME_OUT / 1000;
		tv.tv_usec = RECV_TIME_OUT % 1000 * 1000;
		CHECK(setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));		
		

	}
	if(client_fd >= 0)
		close(client_fd);
}

/////////////////////USER INTERFACE////////////////////////////////////
/*判断有没有客户端连接
 * return: 1－有客户端 0－无客户端
 */
int is_client_connected(void)
{
	return have_client;
}

/*设置服务器监听ip和端口
 * 监听所有ip，ipaddr="0.0.0.0"
 */
void set_pre_serip_port(const char *ipaddr, unsigned short p)
{
	strncpy(pres_ser_ip, ipaddr, IPADDR_LEN);
	pres_ser_port = p;
	DEBUG("set pres server ip:%s port:%d\n", pres_ser_ip, pres_ser_port);
}

/*判断缓冲区中有没有包
 */
inline int have_packet(void)
{
	return !cirbuf_empty(&cb_rcv_ser);
}

/*从缓冲区中获取包
 */
int get_packet(char *buf, int len)
{
	struct sched_pres *sp;
	ASSERT(len>=23);
	CHECK2(copy_cirbuf_to_user_flag(&cb_rcv_ser, buf, 23, 0) == 23);

	sp = (struct sched_pres*)buf;
	int pkt_len = sp->pkt_len;
	if(pkt_len > len)
		return EUSER_BUFF_TOO_SHORT;
	CHECK2(copy_cirbuf_to_user(&cb_rcv_ser, buf, pkt_len) == pkt_len);
	DEBUG("Copy to user:%d bytes\n", pkt_len);
	ASSERT(pkt_len == sp->text_len + sizeof(struct sched_pres));
	return pkt_len;
}

