#ifndef __PLAT_H__
#define __PLAT_H__

/*数据类型定义 */
typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;

#define IPADDR_LEN 16

#define WEB_HEART_BEAT_TIMEDOUT 5
/*返回值 和 错误码定义*/
#define PS_SUCCESS	0
#define PS_FAIL		-1
#define EBUFF_EMPTY	-2
#define EBUFF_FULL	-3
#define EUSER_BUFF_TOO_SHORT -4
#define PS_SEND_ERROR -5

/*----------------------------------------------------
 *平调系统 WEB与区长台语音实时下发/上传(UDP协议)
 */
#define VC_TYPE 0x02
//控制信令
#define CS_CMD		0x01
#define CS_PRESS	0x00
#define CS_RELEASE	0x01
#define CS_UPLOAD	0x02
#define CS_STOP		0x03
#define CS_RECV		0x04
#define CS_DATA_HEART_BEAT 0x02
struct control_sig
{
	uint8 type;		//类型
	uint8 cmd;		//命令
	uint32 time;	//时间戳
	uint8 data;		//数据
}__attribute__((packed));

//语音编码
#define V_CMD	0x02
struct voice
{
	uint8	type;		//类型
	uint8	cmd;		//命令
	uint32	len;		//语音长度
	uint8	data[0];	//语音数据 Data0 ~ Data655
}__attribute__((packed));


/*区长台与应用服务器(TCP)协议
 */

//会话层数据帧格式
#define S_HEART_BEAT 0x0f	//心跳
#define S_DATA		 0x8f	//数据
struct sess
{
	uint32	len;		//长度
	uint8	flag;		//标识 心跳/数据
	uint8	data[0];	//数据
}__attribute__((packed));

//表示层数据格式
struct pres
{
	uint32	dst_tel_code;	//目的电报码
	uint32	src_tel_code;	//源电报码
	uint32	len;			//数据包长度
	uint8	data[0];		//数据内容
}__attribute__((packed));


#define D_TYPE_VOICE 0x04
#define D_TYPE_TEXT 0x05
//详细层数据格式
struct detail
{
	uint32	time;			//时间
	uint8	type;			//数据类型
	uint8	sub_type;		//数据子类型
	uint8	speaker;		//讲话者
	uint8	connector;		//连接员号
	uint32	len;			//数据包长度
	uint8	data[0];		//语音或者文本数据
}__attribute__((packed));

/*每次提交一个任务，如果要发文件，则把文件名
 * 放到file数组中。
 */
#define FILE_NAME_LEN 128
struct pres_task
{
	struct sess		se;
	struct pres		pr;
	struct detail	de;
};
//调车单服务器与区长台(TCP)协议(文本)
//数据格式
#define SP_DATA_TYPE		0x05
#define SP_DATA_SUB_TYPE	0x00
struct sched_pres
{
	uint32	dst_tel_code;	//目的电报码
	uint32	src_tel_code;	//源电报码
	uint32	pkt_len;		//数据包长度
	uint8	no;				//调号
	uint32	time;			//UNIX时间
	uint8	data_type;		//数据类型码
	uint8	data_sub_type;	//数据子类型码
	uint32	text_len;		//文本数据包长度
	uint8	data[0];		//文本数据Data0 ~ DataN
}__attribute__((packed));

/*设置WEB端服务器IP和端口
 * ipaddr: WEB端服务器IP
 * p:	   WEB端服务器端口
 */
void set_webip_port(const char *ipaddr, unsigned short p);

/*设置区长台服务器IP和端口
 * ipaddr: 区长台服务器IP
 * p:	   区长台端服务器端口
 */
void set_preip_port(const char *ipaddr, unsigned short p);

/*启动sig voice 主线程
 */
void start_sig_voice(void);

/*停止主线程
 */
void stop_sig_voice(void);

//获取当前消息类型
#define MSG_NOMSG -1		//缓冲区中无消息
#define MSG_SIGNAL 0		//缓冲区头有一信令
#define MSG_VOICE  1		//缓冲区头有一语音
/*获取当前缓冲区头的消息类型
 *以决定调用那个函数
 */
int get_msg_type(void);


/*获取一个信令消息,信令长度固定，不需要指定长度
 *cs:接受消息缓冲区，由调用者创建
 */
int get_sig(struct control_sig *cs);


/*获取一个语音消息
 *vc:接受消息缓冲区，由调用者创建
 *len:缓冲区大小
 *return: 成功-PS_SUCC len太小-EUSER_BUF_TOO_SHORT 其他错误-FAIL
 */
int get_voice(struct voice *vc, uint32 len);


/*放入发送缓冲区一个信令消息包
 * cs:信令消息结构指针
 * return: PS_SUCCESS-成功 EBUFF_FULL-缓冲区满，放入失败
 *        PS_FAIL-失败，其他原因
 */
int put_sig(struct control_sig *cs);


/*放入发送缓冲区一个语音消息包
 *vc:语音消息结构指针
 *return: PS_SUCCESS-成功 EBUFF_FULL-缓冲区满，放入失败
 *        PS_FAIL-失败，其他原因
 */
int put_voice(struct voice *vc);



void start_tcp_client(void);
void stop_tcp_client(void);
void set_app_ip(const char *ipaddr, unsigned short port);
inline int is_connected(void);
inline int is_busy(void);
int submit_task(char *task, int len, const char *file_name);


void set_pre_serip_port(const char *ipaddr, unsigned short p);
void start_pres_server(void);
void stop_pres_server(void);
inline int have_packet(void);
int get_packet(char *buf, int len);


/*循环缓冲区
 */
//-----!!! 环形缓冲区大小必须是2的n次方(2^n) !!!-----//
#define COPY_ONLY 0 //只复制，不改变缓冲区头尾指针.
#define COPY_CONS 1
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
struct circle_buffer
{
	uint32 head; //缓冲区头
	uint32 tail; //缓冲区尾
	uint8  *buf; //缓冲区
	uint32 size; //缓冲区大小
};

/*初始化环形缓冲区
 * cb:要初始化的环形缓冲区
 * buf:开辟好的空间指针
 * size:开辟好的空间(buf)大小,size必须是2^n
 */
void cirbuf_init(struct circle_buffer *cb, uint8 *buf, int size);

/*判断缓冲区cb是不是为空
 * return: 0-非空 1-空
 */
inline int cirbuf_empty(struct circle_buffer *cb);


/*判断缓冲区cb是不是为满
 * return: 0-不满 1-满
 */
inline int cirbuf_full(struct circle_buffer *cb);

/*获取缓冲区cb空闲大小
 * return:缓冲区空闲空间大小
 */
uint32 cirbuf_get_free(struct circle_buffer *cb);

/*把缓冲区中的数据移动len字节到user中
 * user:用户缓冲区
 * len:用户想移出字节数
 * return:实际移动字节数
 */
int copy_cirbuf_to_user(struct circle_buffer *cb, uint8 *user, uint32 len);
int copy_cirbuf_to_user_flag(struct circle_buffer *cb, uint8 *user, uint32 len, int flag);

/*把user中len字节数据移动到缓冲区中
 * user:用户缓冲区
 * len:用户想移入缓冲区字节数
 * return:实际移动字节数
 */
int copy_cirbuf_from_user(struct circle_buffer *cb, const uint8  *user, uint32 len);

/*设置socfd非阻塞
 */
void setnonblocking(int sockfd);
#endif /*__PLAT_H__*/
