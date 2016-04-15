/*环形缓冲区的实现
 *采用32位无符号整数做为缓冲区头尾指针，
 *利用其自动溢出实现。
 *当只有一个生产者，一个消费者时，使用环形
 *缓冲区是不用加锁保护的。
 */
#include <stdlib.h>

#include "debug.h"
#include "plat_sched.h"

/*-----!!! 环形缓冲区大小必须是2的n次方(2^n) !!!-----
 *初始化环形缓冲区
 * cb:要初始化的环形缓冲区
 * buf:开辟好的空间指针
 * size:开辟好的空间(buf)大小,size必须是2^n
 */
void cirbuf_init(struct circle_buffer *cb, uint8 *buf, int size)
{
	int t = size;
	CHECK2(t > 1);
	while(t > 1)
	{
		CHECK2( (t & 1) == 0 );
		t >>= 1;
	}
	cb->head = cb->tail = 0;
	cb->buf = buf;
	cb->size = size;
}

/*判断缓冲区是否为空
 * return 1-空 0-非空
 */
inline int cirbuf_empty(struct circle_buffer *cb)
{
	return (cb->head == cb->tail);
}


/*判断缓冲区是否为满
 * return 1-满 0-非满
 */
inline int cirbuf_full(struct circle_buffer *cb)
{
	return cb->size == cb->head - cb->tail;
}

/*获取缓冲区中空闲空间大小
 * return 缓冲区空闲空间大小，单位：字节
 */
inline uint32 cirbuf_get_free(struct circle_buffer *cb)
{
	return cb->size - cb->head + cb->tail;
}


/*把缓冲区中数据移动到用户缓冲区中
 * cb:缓冲区
 * user:用户缓冲区指针
 * len:用户想移动的字节数
 * return:真实移动字节数
 */
int copy_cirbuf_to_user(struct circle_buffer *cb, uint8 *user, uint32 len)
{
	return copy_cirbuf_to_user_flag(cb, user, len, COPY_CONS);	
}

/*flag 为0的移动，只是复制len字节到user中，缓冲区尾指针不动，即缓冲区中数据不变
 *flag 为1的移动，缓冲区尾指针向前移动
 *return:返回真实复制或者移动到user中的字节数.
 */
int copy_cirbuf_to_user_flag(struct circle_buffer *cb, uint8 *user, uint32 len, int flag)
{
	uint32 l = 0;
	uint32 head = cb->head;
	uint32 tail = cb->tail;

	len = MIN(len, head - tail);
	l =  MIN(len, cb->size - (tail & (cb->size-1)));
	memcpy(user, cb->buf + (tail & (cb->size-1)), l);
	memcpy(user + l, cb->buf, len -l);
	if(flag)
		cb->tail += len;
	return len;
}

/*把用户缓冲区中的数据复制到环形缓冲区中
 * cb：环形缓冲区
 * user:用户缓冲区
 * len:用户想要复制的字节数
 * return:真实复制字节数
 */
int copy_cirbuf_from_user(struct circle_buffer *cb, const uint8  *user, uint32 len)
{
	uint32 l = 0;
	uint32 head = cb->head;
	uint32 tail = cb->tail;

	len = MIN(len, cb->size - head + tail);
	l = MIN(len, cb->size - (head &(cb->size-1)));
	memcpy(cb->buf + (head & (cb->size-1)), user, l);
	memcpy(cb->buf, user + l, len - l);
	cb->head += len;
	return len;
}
