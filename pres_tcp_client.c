#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "plat_sched.h"
#include "debug.h"

#define DEFUALT_TCP_RCV_TIMEOUT 100000
#define RCV_TCP_BUF_SIZE 4096
#define SND_TCP_BUF_SIZE 4096

static char rcv_tcp_cb_buf[RCV_TCP_BUF_SIZE];
static char snd_tcp_cb_buf[SND_TCP_BUF_SIZE];

static struct circle_buffer cb_rcv_tcp;
static struct circle_buffer cb_snd_tcp;

static char app_ip[IPADDR_LEN]="127.0.0.1";
static unsigned short app_port=5555;

volatile int stop_tc = 0;
int app_fd = -1;
static void handle_tcp_client(void);
static void *tcp_client_thread(void *arg);
/*设置应用服务器ip和端口
 */
void set_app_ip(const char *ipaddr, unsigned short port)
{
	strncpy(app_ip, ipaddr, IPADDR_LEN);
	app_port = port;
	DEBUG("set app ip:%s port:%d\n", app_ip, app_port);
}

pthread_t tcp_client_tid;

int connect_app(void)
{
	struct sockaddr_in app_addr;
	CHECK(app_fd = socket(AF_INET, SOCK_STREAM, 0));
	bzero(&app_addr, sizeof(app_addr));
	app_addr.sin_family = AF_INET;
	app_addr.sin_addr.s_addr = inet_addr(app_ip);
	app_addr.sin_port = htons(app_port);
	CHECK(connect(app_fd, (struct sockaddr*)&app_addr, 
							sizeof(struct sockaddr)));
	setnonblocking(app_fd);
	return app_fd;
}


/*发送数据到应用服务器。
 */
int tcp_client_send_data(void *data, int len)
{
	return send(app_fd, data, len, 0);
}

/*收数据从应用服务器。
 */
int tcp_client_rcv_data(void *data, int len)
{
	return recv(app_fd, data, len, 0);
}
void start_tcp_client(void)
{
	int ret = 0;
	cirbuf_init(&cb_rcv_tcp, rcv_tcp_cb_buf, RCV_TCP_BUF_SIZE);
	cirbuf_init(&cb_snd_tcp, snd_tcp_cb_buf, SND_TCP_BUF_SIZE);
	connect_app();
	CHECK2((ret = pthread_create(&tcp_client_tid, NULL, tcp_client_thread, NULL)) == 0);
	INFO("tcp client started!\n");
}

void stop_tcp_client(void)
{
	stop_tc = 1;
	pthread_join(tcp_client_tid, NULL);
}
static void *tcp_client_thread(void *arg)
{
	handle_tcp_client();
	return NULL;
}

static void handle_tcp_client(void)
{
	while(!stop_tc)
	{

		DEBUG("TCP client thread running...\n");
		usleep(500000);
	}
	
	DEBUG("TCP client stopped!\n");
}
