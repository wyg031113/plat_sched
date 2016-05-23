/*************************************************************************
    > File Name: testB.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2016年04月15日 星期五 08时39分35秒
 ************************************************************************/

int main2();
int main3();
int main6();
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "debug.h"

void *thread1(void *arg)
{
	main2();
	INFO("main2 exited!\n");
	exit(0);
}

void *thread2(void *arg)
{
	main3();
	INFO("main3 exited!\n");
	exit(0);
}

void *thread3(void *arg)
{
	main6();
	INFO("main6 exited!\n");
	exit(0);
}

pthread_t tid1, tid2, tid3;
int main()
{
	CHECK2(pthread_create(&tid1, NULL, thread1, NULL) == 0);
//	CHECK2(pthread_create(&tid2, NULL, thread2, NULL) == 0);
//	CHECK2(pthread_create(&tid3, NULL, thread3, NULL) == 0);

	pthread_join(tid1, NULL);
//	pthread_join(tid2, NULL);
//	pthread_join(tid3, NULL);
}
