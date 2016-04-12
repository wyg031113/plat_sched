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
#define MAX_TASK_LEN 2048

enum send_status
{
	NEW_TASK,
	SENDING_HEADER,
	SENDING_VOICE,
	SENDING_FILE,
	SENDING_FINISHED
};

static char task_buf[MAX_TASK_LEN];
static int task_len;
static enum send_status status;
static char rcv_buf[MAX_TASK_LEN];
static char file[FILE_NAME_LEN];
static struct pres_task *ptsk = (struct pres_task*) task_buf;
static char rcv_tcp_cb_buf[RCV_TCP_BUF_SIZE];
static char snd_tcp_cb_buf[SND_TCP_BUF_SIZE];

static struct circle_buffer cb_rcv_tcp;
static struct circle_buffer cb_snd_tcp;

static char app_ip[IPADDR_LEN]="127.0.0.1";
static unsigned short app_port=5555;

volatile int stop_tc = 0;
volatile int be_connected = 0;
volatile int be_busy = 0;

int app_fd = -1;
static void handle_tcp_client(void);
static void *tcp_client_thread(void *arg);
#define SEND_IDLE 1
#define SEND_BUSY 2
#define SEND_ERROR 3
static int send_packet_in_buffer(void);
/*设置应用服务器ip和端口
 */
void set_app_ip(const char *ipaddr, unsigned short port)
{
	strncpy(app_ip, ipaddr, IPADDR_LEN);
	app_port = port;
	DEBUG("set app ip:%s port:%d\n", app_ip, app_port);
}

inline int is_connected(void)
{
	return be_connected;
}
inline int is_busy(void)
{
	return be_busy;
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
	return connect(app_fd, (struct sockaddr*)&app_addr, 
							sizeof(struct sockaddr));
	setnonblocking(app_fd);
}


/*发送数据到应用服务器。
 */
int tcp_client_send_data(void *data, int len)
{
	int ret = send(app_fd, data, len, 0);
	if(ret == -1 ||(ret==0 && errno != EAGAIN))
	{
		close(app_fd);
		app_fd = -1;
		be_connected = 0;
		return -1;
	}
	else
		return ret;
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
	app_fd = -1;
	while(!stop_tc)
	{
		DEBUG("TCP client thread running...\n");
		if(!be_connected)
		{
			if(connect_app()==0)
				be_connected = 1;
			else
			{
				be_busy = 1;
				DEBUG("Connect failed! error:%s\n", strerror(errno));
			}
		}
		if(!be_connected)
		{
			usleep(100000);
			continue;
		}	

		send_packet_in_buffer();

	}
	
	DEBUG("TCP client stopped!\n");
	if(app_fd > 0)
		close(app_fd);
}

#define FILE_DATA_LEN 1024
static int send_packet_in_buffer(void)
{
	DEBUG("Send user data\n");
	static int send_offset = 0;
	static int len = 0;
	static int file_fd = -1; //注意意外退出时关闭文件
	static struct stat st;
	static int file_len = 0;
	static char file_data[FILE_DATA_LEN];
	static char file_data_len = 0;
	if(!be_busy)
		return SEND_IDLE;
	switch(status)
	{
		case NEW_TASK:		//新任务
			DEBUG("NEW_TASK\n");
			if(ptsk->de.type == D_TYPE_TEXT)//普通文件任务，不用发送文件
			{
				status = SENDING_HEADER;
				len = task_len;
				send_offset = 0;
				return SEND_BUSY;
			}
			else if(ptsk->de.type == D_TYPE_VOICE) //语音任务，要发送语音文件
			{
				//准备文件
				if(access(file, R_OK)!=0)
				{
					INFO("file:%s can't read! errno=%s\n", strerror(errno));
					status = SENDING_FINISHED;
					return SEND_BUSY;
				}
				if(stat(file, &st) !=0)
				{
					INFO("file:%s can't get stat! errno=%s\n", strerror(errno));
					status = SENDING_FINISHED;
					return SEND_BUSY;
				}
				file_fd = open(file, O_RDONLY);
				if(file_fd < 0)
				{
					INFO("Can't open file %s\n", file);
					status = SENDING_FINISHED;
					return SEND_BUSY;
				}

				//计算帧头
				ptsk->se.len = sizeof(struct pres) + sizeof(struct detail) + st.st_size;
				ptsk->pr.len = sizeof(struct detail) + st.st_size;
				ptsk->de.len = st.st_size;

				//语音文件信息
				file_len = st.st_size;
				send_offset = 0;
				len = sizeof(struct sess) + sizeof(struct pres) + sizeof(struct detail);
				status = SENDING_VOICE;
			}
			break;

		case SENDING_HEADER:
			DEBUG("SENDING HEADER\n");
		case SENDING_VOICE://这里只发送帧头
			DEBUG("SENDING_VOICE\n");
			if(len == 0) //帧头发送完毕
			{
				if(status == SENDING_VOICE)
				{
					status = SENDING_FILE; //语音
					send_offset = 0;
					len = file_len;
					file_data_len = 0;
				}
				else //如果不是发语音，则已经完成了任务
				{
					status = SENDING_FINISHED;
				}
				return SEND_BUSY;
			}
			else //发送帧头
			{
				int ret = tcp_client_send_data(task_buf + send_offset, len);
				if(ret == -1)
				{
					status = SENDING_FINISHED;
					if(file_fd != -1 && status == SENDING_VOICE)
					{
						close(file_fd);
						file_fd = -1;
					}
					return SEND_BUSY;
				}
				len -= ret;
				send_offset += ret;
				return SEND_BUSY;
			}
			break;
		case SENDING_FILE: //发送文件
			DEBUG("SENDING FILE");
			if(file_fd != -1 && file_data_len == 0) //读取文件
			{
				int ret = read(file_fd, file_data + file_data_len, FILE_DATA_LEN - file_data_len);
				if(ret <=0)
				{
					close(file_fd);
					file_fd = -1;
				}
				else
					file_data_len += ret;
				send_offset = 0;
			}
			if(len > 0) //发送文件内容
			{
				if(file_data_len<=0) 
				{
					INFO("Send file failed, file size:%d only send:%d\n", file_len, file_len - len);
					status = SENDING_FINISHED;

					if(file_fd != -1)
					{
						close(file_fd);
						file_fd = -1;
					}
				}
				else //已经读取到文件，直接发送
				{
					int ret = tcp_client_send_data(file_data + send_offset, file_data_len);
					if(ret<0)
					{
						status = SENDING_FINISHED;
						INFO("Send file failed, file size:%d only send:%d\n", file_len, file_len - len);
						if(file_fd !=-1)
						{
							close(file_fd);
							file_fd = -1;
						}
					}
					else
					{
						len -= ret;
						send_offset += ret;
						file_data_len -= ret;
					}
					return SEND_BUSY;
				}

			}
			else //len已经为0，文件发送完毕
			{
				status = SENDING_FINISHED;
				if(file_fd != -1)
				{
					close(file_fd);
					file_fd = -1;
				}
			}
			return SEND_BUSY;
			break;
		case SENDING_FINISHED:
			DEBUG("SENDING FINISHED\n");
			if(file_fd >= 0)
			{
				close(file_fd);
				file_fd = -1;
			}
			be_busy = 0;

			return SEND_IDLE;
		default:
			INFO("Bad send status\n");
	}
}


int submit_task(char *task, int len, const char *file_name)
{
	CHECK2(!be_busy);
	if(file_name != NULL)
	{
		if(strlen(file_name)+1 > FILE_NAME_LEN)
		{
			INFO("File name too long\n");
			return PS_FAIL;
		}
	}
	if(len > MAX_TASK_LEN)
	{
		INFO("Task to long\n");
		return PS_FAIL;
	}
	memcpy(task_buf, task, len);
	strcpy(file, file_name);
	status = NEW_TASK;
	task_len = len;
	be_busy = 1;
	return PS_SUCCESS;
}
