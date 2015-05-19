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
	/* 随机产生请求地址 */
		if((fd =open("/tmp/server",O_WRONLY||O_NONBLOCK))<0)
		printf("do_request open fifo failed!\n");
	for(i=0;i<50;i++){
	/* 随机产生请求类型 */
		memAccReq.virAddr = (int)random() % VIRTUAL_MEMORY_SIZE;
		memAccReq.process=random()&1;
		switch (random() % 3)
		{
			
			case 0: //读请求
			{
				memAccReq.reqType = REQUEST_READ;
				printf("process%d产生请求：\n地址：%u\t类型：读取\n",memAccReq.process+1,memAccReq.virAddr);
				break;
			}
			case 1: //写请求
			{
				memAccReq.reqType = REQUEST_WRITE;
				/* 随机产生待写入的值 */
				memAccReq.value = random() % 0xFFu;
				printf("process%d产生请求：\n地址：%u\t类型：写入\t值：%02X\n", memAccReq.process+1,memAccReq.virAddr, memAccReq.value);
				break;
			}
			case 2:
			{
				memAccReq.reqType = REQUEST_EXECUTE;
				printf("process%d产生请求：\n地址：%u\t类型：执行\n",memAccReq.process+1,memAccReq.virAddr);
				break;
			}
			default:
			break;
		}		
		if(write(fd,&memAccReq,DATALEN)<0)
			printf("do_request write fifo failed\n");
	}
		while(1);
		close(fd);
		return 0;
	}
