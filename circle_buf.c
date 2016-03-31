#include <stdlib.h>

#include "debug.h"
#include "plat_sched.h"

//-----!!! 环形缓冲区大小必须是2的n次方(2^n) !!!-----
/*初始化环形缓冲区
 * cb:要初始化的环形缓冲区
 * buf:开辟好的空间指针
 * size:开辟好的空间(buf)大小,size必须是2^n
 */
void cirbuf_init(struct circle_buffer *cb, uint8 *buf, int size)
{
	CHECK2(size > 1);
	while(size > 1)
	{
		CHECK2( (size & 1) == 0 );
		size >>= 1;
	}
	cb->head = cb->tail = 0;
	cb->buf = buf;
	cb->size = size;
}

inline int cirbuf_empty(struct circle_buffer *cb)
{
	return (cb->head == cb->tail);
}
inline int cirbuf_full(struct circle_buffer *cb)
{
	return cb->size == cb->head - cb->tail;
}

inline uint32 cirbuf_get_free(struct circle_buffer *cb)
{
	return cb->size - cb->head + cb->tail;
}


int copy_cirbuf_to_user(struct circle_buffer *cb, uint8 *user, uint32 len)
{
	uint32 l = 0;
	uint32 head = cb->head;
	uint32 tail = cb->tail;

	len = MIN(len, head - tail);
	l =  MIN(len, cb->size - (tail & (cb->size-1)));
	memcpy(user, cb->buf + (tail & (cb->size-1)), l);
	memcpy(user + l, cb->buf, len -l);
	cb->tail += len;
	DEBUG("to user:real_copy: %u bytes, buff_free:%u bytes, buff_used:%u bytes, buff_size:%u byte head:%u tail:%u\n", 
			len, cirbuf_get_free(cb), cb->size - cirbuf_get_free(cb), cb->size, cb->head, cb->tail);
	return len;
}

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
	DEBUG("to buf:real_copy: %u bytes, buff_free:%u bytes, buff_used:%u bytes, buff_size:%u byte head:%u tail:%u\n", 
			len, cirbuf_get_free(cb), cb->size - cirbuf_get_free(cb), cb->size, cb->head, cb->tail);
	return len;
}
