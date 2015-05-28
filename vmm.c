#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "vmm.h"
#include <fcntl.h>
#define first(i) (i)/16
#define second(i) (i)%16/4
#define third(i) (i)%4
/* 页表 */
PageTableItem pageTable[PAGE_SIZE][PAGE_SIZE][PAGE_SIZE];// Simulateof three level page table with a three-dimensional array;
/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;

//MemoryAccessRequest memAccReq_fifo;

int fifo;
unsigned  TIME_COUNTER=0;

PageTableItem find(int i){//解析地址到三级页表中寻址
	return pageTable[first(i)][second(i)][third(i)];
}
/* 初始化环境 */

void do_init()
{
	int i, j;
	srandom(time(NULL));
	for (i = 0; i < PAGE_SUM; i++)
	{
		pageTable[first(i)][second(i)][third(i)].process=i/32;//0-31=process_1(0),32-64=process_2,we can control it!
		pageTable[first(i)][second(i)][third(i)].pageNum = i;
		pageTable[first(i)][second(i)][third(i)].filled = FALSE;
		pageTable[first(i)][second(i)][third(i)].edited = FALSE;
		pageTable[first(i)][second(i)][third(i)].time_count = 0;
		/* 使用随机数设置该页的保护类型 */

		pageTable[first(i)][second(i)][third(i)].proType = random() % 7+1;
	
		
		/* 设置该页对应的辅存地址 */
		pageTable[first(i)][second(i)][third(i)].auxAddr = i * PAGE_SIZE ;
	}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* 随机选择一些物理块进行页面装入 */
		if (random() & 1)
		{
			do_page_in(&pageTable[first(j)][second(j)][third(j)], j);
			pageTable[first(j)][second(j)][third(j)].blockNum = j;
			pageTable[first(j)][second(j)][third(j)].filled = TRUE;
			blockStatus[j] = pageTable[first(j)][second(j)][third(j)].process+1;
		}
		else
			blockStatus[j] = FALSE;
	}
}


/* 响应请求 */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, offAddr;
	unsigned int actAddr;
	
	/* 检查地址是否越界 */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}
	
	/* 计算页号和页内偏移值 */
	pageNum = ptr_memAccReq->virAddr / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("页号为：%u\t页内偏移为：%u\n", pageNum, offAddr);

	/* 获取对应页表项 */
	ptr_pageTabIt = &pageTable[first(pageNum)][second(pageNum)][third(pageNum)];
	
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	
	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("实地址为：%u\n", actAddr);
	
	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
			ptr_pageTabIt->time_count=++TIME_COUNTER;//???
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功：值为%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
			ptr_pageTabIt->time_count=++TIME_COUNTER;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);	
				return;
			}
			/* 向实存中写入请求的内容 */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;			
			printf("写操作成功\n");
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
			ptr_pageTabIt->time_count=++TIME_COUNTER;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}			
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{	
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);
			
			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->time_count = ++TIME_COUNTER;
			
			blockStatus[i] = ptr_pageTabIt->process+1;
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	do_LRU(ptr_pageTabIt);
}

/* 根据LFU算法进行页面替换 */
void do_LRU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min=0xFFFFFFFF, pagestart=pageTable[first(i)][second(i)][third(i)].process*32,page=pagestart;
	
	printf("没有空闲物理块，开始进行LRU页面替换...\n");
	for (i = pagestart, page ; i <pagestart+32 ; i++)
	{
		if (pageTable[first(i)][second(i)][third(i)].filled&&pageTable[first(i)][second(i)][third(i)].time_count < min)
		{
			min = pageTable[first(i)][second(i)][third(i)].time_count;
			page = i;
		}
	}
	printf("选择第%u页进行替换\n", page);
	if (pageTable[first(16)][second(16)][third(16)].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[first(16)][second(16)][third(16)]);
	}
	pageTable[first(16)][second(16)][third(16)].filled = FALSE;
	pageTable[first(16)][second(16)][third(16)].time_count= 0;


	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[first(16)][second(16)][third(16)].blockNum);
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[first(16)][second(16)][third(16)].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	printf("页面替换成功\n");
}

/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%u-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}		
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}


/* 打印页表 */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("process\t页号\t块号\t装入\t修改\t保护\tTime\t辅存\n");
	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%d\t%u\t%u\t%u\t%u\t%s\t%u\t%u\n", pageTable[first(i)][second(i)][third(i)].process+1,i, pageTable[first(i)][second(i)][third(i)].blockNum, pageTable[first(i)][second(i)][third(i)].filled, 
			pageTable[first(i)][second(i)][third(i)].edited, get_proType_str(str, pageTable[first(i)][second(i)][third(i)].proType), 
			pageTable[first(i)][second(i)][third(i)].time_count, pageTable[first(i)][second(i)][third(i)].auxAddr);
	}
}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}
void initFile(){//-----------------------------------------add1
	int i;
	for(i=0;i<256;i++)
	{

		fprintf(ptr_auxMem, "%c",(int)random() % 26+'a' );
	}
}	
int main(int argc, char* argv[])
{
	char c;
	int i,count;
	struct stat statbuf;
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "w+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	initFile();
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* 在循环中模拟访存请求与处理过程 */
	if(stat("/tmp/server",&statbuf)==0){
		/* 如果FIFO文件存在,删掉 */
		if(remove("/tmp/server")<0)
			printf("remove fifo failed\n");
	}

	if(mkfifo("/tmp/server",0666)<0)
		printf("mkfifo failed!\n");
		/* 在非阻塞模式下打开FIFO */
	if((fifo=open("/tmp/server",O_RDONLY))<0)
		printf("Open fifo failed!\n");

	while (TRUE)
	{
		if((count=read(fifo,ptr_memAccReq,DATALEN))<0)
			printf("Read fifo failed!\n");
		printf("addr:%d\n",ptr_memAccReq->virAddr);
		do_response();
	/*	printf("按Y打印页表，按其他键不打印...\n");
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
		while (c != '\n')
			c = getchar();*/
		printf("按X退出程序,按Y打印页表，按其他键继续...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		else if(c=='Y'||c=='y'){
			do_print_info();
			
		}
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
