#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "vmm.h"
#include <fcntl.h>
int main(int argc,char *argv[])
{
	int fd,i;
	MemoryAccessRequest memAccReq;
	srandom(time(NULL));
	
	if((fd =open("/tmp/server",O_WRONLY||O_NONBLOCK))<0)
		printf("give_request open fifo failed!\n");
	
		memAccReq.virAddr =atoi(argv[1]);
		memAccReq.process=memAccReq.virAddr/128;
		switch (argv[2][0])
		{
			
			case 'r': //读请求
			{
				memAccReq.reqType = REQUEST_READ;
				printf("process%d产生请求：\n地址：%u\t类型：读取\n",memAccReq.process+1,memAccReq.virAddr);
				break;
			}
			case 'w': //写请求
			{
				memAccReq.reqType = REQUEST_WRITE;
				memAccReq.value =argv[3][0];
				printf("process%d产生请求：\n地址：%u\t类型：写入\t值：%02X\n", memAccReq.process+1,memAccReq.virAddr, memAccReq.value);
				break;
			}
			case 'e':
			{
				memAccReq.reqType = REQUEST_EXECUTE;
				printf("process%d产生请求：\n地址：%u\t类型：执行\n",memAccReq.process+1,memAccReq.virAddr);
				break;
			}
			default:
			break;
		}		
		if(write(fd,&memAccReq,DATALEN)<0)
			printf("give_request write fifo failed\n");
	

	close(fd);
	return 0;
}
