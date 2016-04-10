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

	CHECK(start_listen()); //启动主线程

	while(!stop_server)
	{
		DEBUG("run.., len = %d\n", len);
		tfd = accept(pres_serfd, NULL, NULL); //接收客户端连接，由于pers_serfd设置为非阻塞，会立即返回
		if(tfd < 0)
		{
			if(errno == EAGAIN)				//没有新客户端连接
			{
				DEBUG("Accept again\n");
				if(client_fd == -1)         //没有旧客户端要处理，sleep
					usleep(SLEEP_TIME);
			}
			else
				CHECK(tfd);
		}
		else								//有新连接到来
		{
			if(client_fd > 0)				//由于只有一个客户端，所有断定上个连接已经终止，关闭socket
			{
				close(client_fd);
				client_fd = -1;
			}

			DEBUG("accept a new client\n");
			client_fd = tfd;
			have_client = 1;
			//设置超时
			tv.tv_sec = RECV_TIME_OUT / 1000;
			tv.tv_usec = RECV_TIME_OUT % 1000 * 1000;
			CHECK(setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));	
		}

		if(client_fd > 0 && last_success) //有客户端，且上一次接收是成功的
		{
			ret = recv(client_fd, tmp_rcv_buf+len, MAX_SEG_SIZE-len, 0); //继续接收数据
			if(ret == 0)				  //客户端断开连接
			{
				close(client_fd);
				client_fd = -1;
				have_client = 0;
				len = 0;
			}
			else if(ret <0)				 
			{
				if(errno == EAGAIN)		//客户端没有数据，接收超时
				{
					DEBUG("server recv timed out!\n");
				}
				else				   //客户端出现异常
				{
					close(client_fd);
					client_fd = -1;
					have_client = 0;
					len = 0;
				}
			}
			else
			{
				struct sched_pres *sp = (struct sched_pres*)tmp_rcv_buf;
				len += ret;
				if(len < 23)
					continue;
				if(sp->pkt_len < 23 || sp->pkt_len != sp->text_len + 23 || sp->pkt_len > RCV_CB_SER_BUF_SIZE )
				{
					len = 0;
					close(client_fd);
					client_fd = -1;
					INFO("Recv bad packet! just close connection!\n");
					continue;
				}
				CHECK2(sp->pkt_len <= MAX_SEG_SIZE);
				if(len < sp->pkt_len)
					continue;
				DEBUG("Recv new packet!\n");
				int i;
			uint8 c = 0;
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
		}

		//放到环形缓冲区
		if(len > 0)		
		{
			ret = check_add_cb(tmp_rcv_buf, len);
			last_success = 1;

			/*如果可以放到一个完整的包则放入，否则一个字节也不放入，保证完整
			 */
			if(ret <=0)			  //由于缓冲区满，可能不能完全放入，这时应该等待
			{
				last_success = 0; //用户取走缓冲区中的内容，并不从socket中读数据				
				DEBUG("circle buf full!!\n");
			}
			else
			{
					len -= ret;
			}
			if(!last_success)    //依然失败，睡眠等待
			{
				usleep(10000);
			}
		}

	}
	if(client_fd > 0)
		close(client_fd);
}

int check_add_cb(char *buf, int len)
{
	DEBUG("Add to circle buffer\n");
	struct sched_pres *sp = (struct sched_pres*)buf;
	int pkt_len = 0;
	if(len<23 || sp->pkt_len > len)
	{
		DEBUG("sp->pkt_len > len\n");
		return 0;

	}
	CHECK2(sp->pkt_len <= RCV_CB_SER_BUF_SIZE);
	if(sp->pkt_len > cirbuf_get_free(&cb_rcv_ser))
	{
		DEBUG("circle buff full...\n");
		return 0;
	}
	pkt_len = sp->pkt_len;
	CHECK2(copy_cirbuf_from_user(&cb_rcv_ser, buf, pkt_len) == pkt_len);
	memmove(buf, buf + pkt_len, len - pkt_len);
	return pkt_len;
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
inline int have_packet(void)
{
	return !cirbuf_empty(&cb_rcv_ser);
}

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
	return pkt_len;
}

