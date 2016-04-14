#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include "plat_sched.h"
#include "debug.h"

#define DEFUALT_TCP_RCV_TIMEOUT 100000
#define MAX_TASK_LEN 2048
#define MAX_FILE_DATA_LEN 1024

#define RCV_TCP_BUF_SIZE 4096		//接收缓环形冲区大小,必须是２^n
static char rcv_buf_tcp[RCV_TCP_BUF_SIZE];
struct circle_buffer cb_rcv_tcp;

enum task_status
{
	TASK_NEW,
	TASK_RUNNING,
	TASK_WAITING,
	TASK_SUCCESS,
	TASK_FAIL
};

enum task_status status;			//任务当前状态
static char task_buf[MAX_TASK_LEN]; //提交任务保存到这里
static int task_len;				//提交的任务长度
static char file[FILE_NAME_LEN];	//提交的文件名
static char file_data[MAX_FILE_DATA_LEN]; //读取文件临时缓冲
static struct pres_task *ptsk = (struct pres_task*) task_buf; //task_buf结构体
static char heart_beat_pkt[16];		//要发送的心跳包
time_t last_send_heart;				//上一次发送心跳包的时间
time_t last_rcv_heart;				//上一次发送心跳包的时间

static char rcv_buf[MAX_TASK_LEN];
static char tmp_rcv_buf[MAX_TASK_LEN];

static char app_ip[IPADDR_LEN]="127.0.0.1";
static unsigned short app_port=5555;

volatile int stop_tc = 0;			//是否停止服务器
volatile int be_busy = 0;			//是否正在发送新任务

int app_fd = -1;					//服务器socket
struct sockaddr_in app_addr;

//线程和对应TID
static pthread_t tcp_client_read_tid;
static pthread_t tcp_client_write_tid;
static void *tcp_client_read_thread(void *arg);
static void *tcp_client_write_thread(void *arg);
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*设置应用服务器ip和端口
 */
void set_app_ip(const char *ipaddr, unsigned short port)
{
	strncpy(app_ip, ipaddr, IPADDR_LEN);
	app_port = port;
	DEBUG("set app ip:%s port:%d\n", app_ip, app_port);
}

/*判断是否连接到服务器
 * return 1-已经连接  0－未连接
 */
inline int is_connect(void)
{
	return connect_app() == 0;
}

/*判断当前有没有任务
 * 返回：0－没有 1－有
 */
inline int is_busy(void)
{
	return be_busy;
}

/*判断有没有收到包
 */
inline int have_pkt(void)
{
	return !cirbuf_empty(&cb_rcv_tcp);
}
/*获取当前任务状态
 * return 返回状态码，参见enum task_status.
 */
inline enum task_status get_status(void)
{
	return status;
}

/*处理sig_pipe信号
 */
void sig_pipe(int sig)
{
	close(app_fd);
	CHECK(app_fd = socket(AF_INET, SOCK_STREAM, 0));
}

/*保证连接到服务器才返回
 *return: 连接失败0，连接成功非0
 */
int connect_app(void)
{
	int ret = 0;	
	while(!stop_tc)
	{
		ret =  connect(app_fd, (struct sockaddr*)&app_addr, sizeof(struct sockaddr));
		if(ret != 0)
		{
			if(errno == EISCONN)
			{
				ret = 0;
				break;
			}
			INFO("Connect failed! error=%s\n", strerror(errno));
			usleep(1000000);
		}
		else
			break;
	}
	//setnonblocking(app_fd);
	return ret == 0;
}


/*发送数据到应用服务器。
 *这里保证发完len字节才返回，除非要停止了
 *返回已经发送的字节数
 */
int tcp_client_send_data(char *data, int len)
{
	int tlen = len;
	int offset = 0;
	while(tlen > 0 && ! stop_tc)
	{

		int ret = send(app_fd, data + offset, tlen, 0);
		if((ret == -1 && errno != EAGAIN) || ret == 0)
		{
			DEBUG("SEND error:%s\n", strerror(errno));
			break;
		}
		else
		{
			tlen -= ret;
			offset += ret;
		}
	}
	return len - tlen;
}

/*收数据从应用服务器。
 * 接收够len字节才返回，除非要停止
 * return 实际接收字节数
 */
int tcp_client_rcv_data(void *data, int len)
{
	int offset = 0;
	int ret;
	while(offset < len && !stop_tc)
	{
		ret = recv(app_fd, data, len, 0);
		if((ret == -1 && errno != EAGAIN) || ret == 0)
		{
			raise(SIGPIPE);
			break;
		}
		else if(ret == -1 && errno == EAGAIN)
		{
			if(time(NULL) - last_rcv_heart > WEB_HEART_BEAT_TIMEDOUT)
			{

				raise(SIGPIPE);
				break;
			}

		}
		else
		{
			offset += ret;
			last_rcv_heart = time(NULL);
		}
	}
	return offset;
}

/*初始化要发送的心跳包
 */
void init_heart_beat_pkt(void)
{

	struct sess *ss = (struct sess*)heart_beat_pkt;
	//初始化心跳包
	ss->len = 11;
	ss->flag = S_HEART_BEAT;
	memset(ss->data, 0xff, ss->len);
}

/*初始化用到的数据结构
 */
void start_tcp_client(void)
{
	int ret = 0;
	app_fd = -1;
	stop_tc = 0;

	signal(SIGPIPE, sig_pipe);

	CHECK(app_fd = socket(AF_INET, SOCK_STREAM, 0));
	bzero(&app_addr, sizeof(app_addr));
	app_addr.sin_family = AF_INET;
	app_addr.sin_addr.s_addr = inet_addr(app_ip);
	app_addr.sin_port = htons(app_port);
	
	init_heart_beat_pkt();

	cirbuf_init(&cb_rcv_tcp, rcv_buf_tcp, RCV_TCP_BUF_SIZE);
	CHECK2((ret = pthread_create(&tcp_client_read_tid, NULL, tcp_client_read_thread, NULL)) == 0);
	CHECK2((ret = pthread_create(&tcp_client_write_tid, NULL, tcp_client_write_thread, NULL)) == 0);
	INFO("tcp client started!\n");
}

/*停止所有线程
 */
void stop_tcp_client(void)
{
	stop_tc = 1;
	pthread_join(tcp_client_read_tid, NULL);
	pthread_join(tcp_client_write_tid, NULL);
	close(app_fd);
}

/*从TCP服务器收包
 */
static void *tcp_client_read_thread(void *arg)
{
	char buf[128];
	int ret;
	int need_connect = 1;
	int offset = 0;;
	int last_success = 1;
	struct sess *ss = (struct sess*)tmp_rcv_buf;
	struct pres_task *pt = (struct pres_task*)tmp_rcv_buf;
	while(!stop_tc)
	{
		if(need_connect)
		{
			if(!connect_app())
				continue;
			else
				need_connect = 0;
		}

		if(!last_success)
		{
			if(cirbuf_get_free(&cb_rcv_tcp) < offset)
			{
				sleep(100000);
				continue;
			}
			CHECK2(copy_cirbuf_from_user(&cb_rcv_tcp, tmp_rcv_buf, offset));
			last_success = 0;
		}
		//接收头
		offset = 0;
		ret = tcp_client_rcv_data(tmp_rcv_buf, sizeof(struct sess));
		if(ret != sizeof(struct sess))
		{
			need_connect = 1;
			raise(SIGPIPE);
			continue;
		}
		else
			offset += ret;

		if(ss->flag == S_HEART_BEAT) //心跳包
		{
			last_rcv_heart = time(NULL);
			if(tcp_client_rcv_data(tmp_rcv_buf+offset, 11) != 11)
			{
				need_connect = 1;
				raise(SIGPIPE);
				continue;
			}
		}
		else						 //普通数据包
		{
			if(ss->len + sizeof(struct sess) > MAX_TASK_LEN)
			{
				INFO("Recv a too long packet: len = %d\n", ss->len);
				raise(SIGPIPE);
				need_connect = 1;
				continue;
			}

			if(tcp_client_rcv_data(tmp_rcv_buf + offset, ss->len) != ss->len)
			{
				raise(SIGPIPE);
				need_connect = 1;
				continue;
			}
			last_success = 0;

		}	
	}
	
	DEBUG("TCP client read stopped!\n");
}

int do_task(void);
/*发送心跳和提交过来的任务
 */
static void *tcp_client_write_thread(void *arg)
{
	int need_connect = 1;
	while(!stop_tc)
	{
		if(need_connect) //没有连接到服务器，要重新连接
		{
			DEBUG("connecting....\n");
			if(!connect_app())
				continue;
			else
				need_connect = 0;
		}
		if(!be_busy)	//没有任务，空闲，发心跳包
		{
			if(time(NULL) - last_send_heart > HEART_BEAT_INTV)
			{
				if(tcp_client_send_data(heart_beat_pkt, sizeof(heart_beat_pkt)) != sizeof(heart_beat_pkt))
					need_connect = 1;
				last_send_heart = time(NULL);
			}
			else
				usleep(500000);
		}
		else		//有任务
		{
			DEBUG("Do task...\n");
			if(do_task() == PS_SEND_ERROR)
			{
				need_connect = 1;
				DEBUG("任务失败，需要重新连接服务器!\n");
			}
			be_busy = 0;
			last_send_heart = time(NULL);
		}
	}
	DEBUG("TCP client write stopped!\n");
}

int send_file(const char *file_name, int len);
int do_task(void)
{
	struct stat st;
	int header_len = 0;
	status = TASK_RUNNING;
	ASSERT(be_busy);
	if(ptsk->de.type == D_TYPE_TEXT)//普通文本任务，不用发送文件
	{
		if(tcp_client_send_data(task_buf, task_len) != task_len)
		{
			status = TASK_FAIL;
			return PS_SEND_ERROR;
		}
		status = TASK_SUCCESS;
	}
	else if(ptsk->de.type == D_TYPE_VOICE) //语音任务，要发送语音文件
	{
		//准备文件
		if(access(file, R_OK)!=0)
		{
			INFO("file:%s can't read! errno=%s\n", file,  strerror(errno));
			status = TASK_FAIL;
			return PS_FAIL;
		}
		if(stat(file, &st) !=0)
		{
			INFO("file:%s can't get stat! errno=%s\n", strerror(errno));
			status = TASK_FAIL;
			return PS_FAIL;
		}
		//计算帧头
		ptsk->se.len = sizeof(struct pres) + sizeof(struct detail) + st.st_size;
		ptsk->pr.len = sizeof(struct detail) + st.st_size;
		ptsk->de.len = st.st_size;

		//发送帧头
		header_len = sizeof(struct sess) + sizeof(struct pres) + sizeof(struct detail);	
		if(tcp_client_send_data(task_buf, header_len) != header_len)
		{
			INFO("send header failed!\n");
			status = TASK_FAIL;
			return PS_SEND_ERROR;
		}

		//发送文件
		if(send_file(file, st.st_size) != st.st_size)
		{
			INFO("send  file failed!\n");
			status = TASK_FAIL;
			return PS_SEND_ERROR;
		}		

		status = TASK_SUCCESS;
	}
	return PS_SUCCESS;
}

/*发送文件，保证文件发送完成后返回
 * 除非出错
 * return: 实际发送文件字节数
 */
int send_file(const char *file_name, int len)
{
	int file_fd;
	int tlen = len;
	file_fd = open(file, O_RDONLY);
	if(file_fd < 0)
	{
		INFO("Can't open file %s\n", file);
		return PS_FAIL;
	}
	while(len > 0)
	{
		int ret = read(file_fd, file_data, MAX_FILE_DATA_LEN);
		if(ret <= 0)
		{
			close(file_fd);
			return tlen - len;
		}
		if(tcp_client_send_data(file_data, ret) != ret)
		{
			close(file_fd);
			return tlen - len;
		}
		len -= ret;
	}
	return tlen - len;
}

/*
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

*/
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
		strcpy(file, file_name);
	}
	if(len > MAX_TASK_LEN)
	{
		INFO("Task to long\n");
		return PS_FAIL;
	}
	memcpy(task_buf, task, len);
	status = TASK_NEW;
	task_len = len;
	be_busy = 1;
	return PS_SUCCESS;
}

/*获取一个帧
 * return: 成功返帧长度 失败返回原因
 */
int get_frame(char *buf, int len)
{
	struct pres_task *pt = (struct pres_task *) buf;
	int dlen = 0;
	if(cirbuf_empty(&cb_rcv_tcp))
	{
		return EBUFF_EMPTY;
	}
	
	if(len < sizeof(struct sess))
		return EUSER_BUFF_TOO_SHORT;
	CHECK2(copy_cirbuf_to_user_flag(&cb_rcv_tcp, buf, sizeof(struct sess), 0) == sizeof(struct sess));
	
	dlen = pt->se.len + sizeof(struct sess);
	if(dlen > len)
		return EUSER_BUFF_TOO_SHORT;
	CHECK2(copy_cirbuf_to_user(&cb_rcv_tcp, buf, dlen) == dlen);
	return dlen;
}
