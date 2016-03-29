#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
/*调试相关*/

/*如果开启DEBUG_TO_LOG宏，则调试信息输出到日志中
 *否则调试信息输出到标准错误中
*/
//#define DEBUG_TO_LOG 

//等级，0-关闭所有输出 1-只输出INFO 2-输出INFO和DEBUG
//调试时设置为2，最后部署运行时设置为1
#define DEBUG_ON

#ifdef DEBUG_TO_LOG
	#ifdef DEBUG_ON /*输出DEBUG和INFO信息到日志*/
		#define DEBUG(format, ...) DEBUG_LOG(format, ##__VA_ARGS__)
	#else	
		#define DEBUG(format, ...)
	#endif

	/*输出INFO信息到日志*/
	#define INFO(format, ...) INFO_LOG(format, ##__VA_ARGS__)
		
#else
	#ifdef DEBUG_ON  /*输出DEBUG和INFO信息到标准错误流和标准输出流*/
		#define DEBUG(format, ...) DEBUG_MSG(format, ##__VA_ARGS__)
	#else
		#define DEBUG(format, ...)
	#endif

	 /*输出INFO信息到标注输出*/
	#define INFO(format, ...)  INFO_MSG(format, ##__VA_ARGS__)
	
#endif /*DEBUG_TO_LOG*/


//打开系统日志
#define OPEN_LOG() openlog("DEBUG_LOG", LOG_CONS|LOG_PID,0)

//以LVL级别输出日志
#define SYSLOG(LOG_LVL,format, ...) do{\
	syslog(LOG_LVL, "%s(%d)-%s:"format, __FILE__, __LINE__, __FUNCTION__,\
			##__VA_ARGS__);\
}while(0);

//调试日志
#define DEBUG_LOG(format, ...) 	SYSLOG(LOG_DEBUG, format, ##__VA_ARGS__)
//消息日志
#define INFO_LOG(format, ...) 	SYSLOG(LOG_INFO, format, ##__VA_ARGS__)

//输出信息到file
#define MSG(file, format, ...)do{\
	fprintf(file, "%s(%d)-%s():"format, __FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
}while(0)

//普通消息
#define INFO_MSG(format, ...) MSG(stdout, format,##__VA_ARGS__)

//调试消息
#define DEBUG_MSG(format, ...) MSG(stderr, format,##__VA_ARGS__)


/*为来简单化一些系统调用,这里定义了几个宏来检查返回值。*/

//打印出错原因，并退出进程
#define err_quit(x)\
do{\
	INFO("CHECK<%s> failed!     errno:%s\n", #x, strerror(errno));\
	exit(1);\
}while(0)

//x是-1,输出errno,并退出
#define CHECK(x) do{if((int)(x)==-1){\
	err_quit(x);\
	}\
}while(0)


//x是false,输出errno,并退出
#define CHECK2(x) do{if(!(x)){\
	err_quit(x);\
	}\
}while(0)

//注意，断言操作关闭调试后失效
#ifdef DEBUG_ON
#define ASSERT(x) do{\
	if(!(x))\
		err_quit(x);\
}while(0)
#else
#define ASSERT(x)
#endif
#endif /*__DEBUG_H__*/
