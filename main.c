/*************************************************************************
    > File Name: main.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2016年03月28日 星期一 19时01分50秒
 ************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include "debug.h"
#include "plat_sched.h"

void check_data_type()
{
	printf("  结构             \t大小(字节)\n");
	printf("struct control_sig:\t%d\n",   sizeof(struct control_sig));
	printf("struct voice:\t\t%d\n",		  sizeof(struct voice));
	printf("struct sess:\t\t%d\n",        sizeof(struct sess));
	printf("struct pres:\t\t%d\n",        sizeof(struct pres));
	printf("struct detail:\t\t%d\n",      sizeof(struct detail));
	printf("struct sched_pres:\t%d\n",    sizeof(struct sched_pres));
}
void test_debug()
{
	int y  = 3;
	int z = 334;
	int i;
	pid_t pid;
	INFO("TEST DEBUG......%d\n",y);
	DEBUG("DEBUG:%d %d\n", y, z);
	DEBUG("DEBUG HELLO\n");
	INFO("TEST DEBUG END\n");
	for(i = 0; i < 3; i++)
	{
		pid = fork();
		if(pid == 0)
		{
			if(i == 0)
				CHECK(y = open("sdf", 0));
			else if(i == 1)
				CHECK2(y = 0);
			else if(i == 2)
				ASSERT(z == 0);
			INFO("Can't reach here!!!!!!\n'");
		}
		wait(NULL);
	}
}
int main()
{
	check_data_type();	
	test_debug();
	return 0;
}
