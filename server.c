#include  <sys/socket.h>
#include  <stdio.h>
#include  <unistd.h>
#include  <arpa/inet.h>
#include  <netinet/in.h>
#include  <errno.h>
#include  <pthread.h>
#include  <sys/types.h>
#include  <stdlib.h>
#include  <string.h>
#include  <sys/epoll.h>
#include  <fcntl.h>
#include  <malloc.h>
#include    "head.h"
#define		SERV_PORT 	7979
#define     SERV_ADDR	"10.12.43.29"
#define		client_num	100
#define     to_close	0
#define     sucess		1

//任务队列和返回值队列
ret_link *ret_head, *ret_p1, *ret_tail;
task *task_head, *task_p1, *task_tail;

//事件结构体,ev:用于注册事件，event数组：用于回传要处理的事件
struct epoll_event ev, events[client_num];
//设置继电器标志
int to_set_key = 0, epfd;
extern int errno;
int exception = 0;
unsigned char *status;
unsigned char address[254];
unsigned char erroraddr[254];
int board_num = 0, init = 0, status_length = 0;

//读写锁
pthread_rwlock_t taskptr, retptr, statusptr, addressptr, errorptr;
pthread_mutex_t searchptr;
//功能：设置套接字为非阻塞模态
//入口：套接字描述符
//出口：成功返回0，失败返回-1
int set_non_blocking(int sock)
{
	int opts;
	//获得套接字的file flag
	if((opts = fcntl(sock, F_GETFL)) == -1)
	{
		perror("get file flag wrong");
		return -1;
	}
	//设置为非阻塞形式
	opts |= O_NONBLOCK;
	if(fcntl(sock, F_SETFL, opts) == -1)
	{
		perror("set file flag wrong");
		return -1;
	}
	return 0;
}

const unsigned char auchCRCHi[] = 
{
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40
} ;

const unsigned char auchCRCLo[] = 
{
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
    0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
    0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
    0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
    0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
    0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
    0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
    0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
    0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
    0x40
};
int main(int argc, char *argv[])
{		

	//串口线程号
	pthread_t seri_pid = -1;
	int nfds, i, j, k, t, on = 1, cmd_length, opt;
	ret_link * ret_test;
	task *task_test;
	//存储网络命令
	unsigned char cmd_temp[100], cmd[60], address_flag[65], address_temp[33];
	uint CRCDATA;
	//服务器地址
	struct sockaddr_in servaddr;
	struct sockaddr_in clientaddr;
	
	int listenfd, connfd;
	socklen_t len;
	to_set_key = 0;
	//读初始化文件
	printf("read init file...\n");
	if(read_init(&board_num, address_flag) == -1)
	  return -1;
	status_length = (board_num % 8) ? (board_num / 8 + 1) : (board_num / 8);
	j = strlen(address_flag);
	ctoi(address_flag, address_temp, &j);
	save_addr(address_temp, board_num, j + 5);
	for(j = 0; j < board_num; j++)
	{
		printf("%d, ", address[j]);
	}
	printf("status size: %d\n", status_length);
	status = (unsigned char *)malloc(status_length * sizeof(unsigned char));
	memset(status, 0, status_length);
	init = 1;
	//创建套接字,设置为非阻塞模式
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	set_non_blocking(listenfd);
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	//设置地址结构
	bzero(&clientaddr, sizeof(clientaddr));
	bzero(&servaddr, sizeof(servaddr));
	len = sizeof(clientaddr);
	servaddr.sin_family = AF_INET;

	//将主机字节序转换为网络字节序
	servaddr.sin_port = htons(SERV_PORT);
	
	//设置服务器IP
	if(argc == 2)
	{
		inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
	}else{

		inet_pton(AF_INET, SERV_ADDR, &servaddr.sin_addr);
	}
	
	//绑定，监听
	if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
	{
		printf("bind error: %s\n", strerror(errno));
		return -1;
	}
	
	if(listen(listenfd, client_num) == -1)
	{
		printf("listen error: %s\n", strerror(errno));
		return -1;
	}
	printf("server has been listened\n");

	//创建任务队列和返回队列
	ret_head = (ret_link *) malloc(sizeof(ret_link));
	ret_head->next = NULL;
	ret_tail = ret_head;
	task_head = (task *) malloc(sizeof(task));
	task_head->next = NULL;
	task_tail = task_head;
	printf("task link and return link created done\n");

	//创建epoll句柄，
	epfd = epoll_create(client_num);

	//添加监听套接字
	ev.data.fd = listenfd;
	
	//添加监听套接字可读事件
	ev.events = EPOLLIN;

	//注册epoll事件
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
	printf("epoll file created done\n");

	//初始化读写锁
	pthread_rwlock_init(&retptr, NULL);
	pthread_rwlock_init(&taskptr, NULL);
	pthread_rwlock_init(&statusptr, NULL);
	pthread_rwlock_init(&addressptr, NULL);
	pthread_rwlock_init(&errorptr, NULL);
	pthread_mutex_init(&searchptr, 	NULL);
	printf("reader and writer lock initialized done\n");
	if(seri_pid == -1)
	{
		while(pthread_create(&seri_pid, NULL, comm_seri, NULL) != 0);	  
			printf("serial thread has created\n");
	}
	while(1)
	{
		//阻塞等待epoll事件发生
		nfds = epoll_wait(epfd, events, client_num, -1);
		printf("wait for events\n");
		for(i = 0; i < nfds; i++)
		{
			printf("events %d\n",i);
			//新连接到来
			if(events[i].data.fd == listenfd)
			{
				//accept新连接
				printf("new\n");
				if((connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &len)) == -1)
				{
					printf("accept error\n", strerror(errno));
					continue;
				}else{
					printf("accept a new client: %s, %d\n", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
					set_non_blocking(connfd);	
					//注册事件
					ev.data.fd = connfd;
					ev.events = EPOLLIN | EPOLLET;
					epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
					printf("listen input event\n");
				}
		
			}
			else if(events[i].events & EPOLLIN)
			{
				//可读, 读数据，获得指令
				printf("net data arrive\n");
				opt = get_net_data(events[i].data.fd, cmd_temp);		
			
				switch(opt)
				{
					case to_close:
						bzero(cmd_temp, sizeof(cmd_temp));
						//掉线则删除任务和返回节点
						pthread_rwlock_wrlock(&taskptr);
						task_p1 = task_head;
						task_test = task_p1->next;
						while(task_test != NULL)
						{
							if(task_test->fd != events[i].data.fd)
							{
								task_p1 = task_test;
								task_test = task_test->next;
							}else{
								//删除
								task_p1->next = task_test->next;
								printf("remove task\n");
								free(task_test);
								task_test = NULL;
								if(task_p1->next == NULL)
								  task_tail = task_p1;
								task_test = task_p1->next;
								
							}
						}
						pthread_rwlock_unlock(&taskptr);
						pthread_rwlock_wrlock(&retptr);
						ret_p1 = ret_head;
						ret_test = ret_p1->next;
						while(ret_test != NULL)
						{
							if(ret_test->sockfd != events[i].data.fd)
							{
								ret_p1 = ret_test;
								ret_test = ret_test->next;
							}else{
								//删除
								ret_p1->next = ret_test->next;
								printf("remove return\n");
								free(ret_test);
								ret_test = NULL;
								if(ret_p1->next == NULL)
								  ret_tail = ret_p1;
								ret_test = ret_p1->next;
							}
						}
						pthread_rwlock_unlock(&retptr);
						ev.data.fd = events[i].data.fd;
						ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
						if(epoll_ctl(epfd, EPOLL_CTL_DEL, ev.data.fd, &ev) == -1)
						  printf("delet event fail\n");
						else
						  printf("delet event\n");
						close(events[i].data.fd);
						printf("net linkdown, close socket %d\n",events[i].data.fd);
						break;
					case sucess:
						printf("received net data from fd %d, cmd: %s\n", events[i].data.fd, cmd_temp);	
						cmd_length = strlen(cmd_temp);//获取命令长度
						//转换成整型
						bzero(cmd, sizeof(cmd));
						ctoi(cmd_temp, cmd, &cmd_length);
						//CRC校验位
						CRCDATA = getCRC16(cmd, cmd_length - 2);
						if(CRCDATA != (*(cmd + cmd_length - 2) << 8 | *(cmd + cmd_length - 1)))
						{
							//CRC错误则清空
							printf("crc wrong\n");
							bzero(cmd, sizeof(cmd));
							bzero(cmd_temp, cmd_length);
						}
						else{
							printf("cmd is :");
							//判断操作类型
							if(*cmd == 0x01)
							{
								//扫描节点状态，增加返回链表项
								printf("to search status\n");
								ret_p1 = (ret_link *)malloc(sizeof(ret_link));
								ret_p1->sockfd = events[i].data.fd;
								ret_p1->next = NULL;
								//构建返回值
								ret_p1->ret[0] = 0x01;
								//复制状态
								ret_p1->ret[2] = status_length;
								pthread_rwlock_rdlock(&statusptr);
								memcpy(ret_p1->ret + 3, status, ret_p1->ret[2]);
								pthread_rwlock_unlock(&statusptr);
								pthread_rwlock_rdlock(&errorptr);
								if(exception == 0)
								{
									ret_p1->ret[1] = 0x00;
									//添加CRC校验位
									CRCDATA = getCRC16(ret_p1->ret, ret_p1->ret[2] + 3);
									ret_p1->ret[ret_p1->ret[2] + 3] = (CRCDATA >> 8) & 0xFF;
									ret_p1->ret[ret_p1->ret[2] + 4] = CRCDATA & 0xFF;
									//转换成字符形式
									itoc(ret_p1->ret, ret_p1->ret[2] + 5);
								}
								else
								{
									//复制异常地址
								
									ret_p1->ret[1] = exception;
									for(j = 0, k = 0; (j < 254) && (k < exception); j++)
									{
										if(erroraddr[j] == 1)
										{
											ret_p1->ret[3 + status_length + k] = j + 1;
											k++;
										}
									}
									//添加CRC校验位
									CRCDATA = getCRC16(ret_p1->ret, ret_p1->ret[2] + 3 + exception);
									ret_p1->ret[ret_p1->ret[2] + 3 + exception] = (CRCDATA >> 8) & 0xFF;
									ret_p1->ret[ret_p1->ret[2] + 4 + exception] = CRCDATA & 0xFF;
									//转换成字符形式
									itoc(ret_p1->ret, ret_p1->ret[2] + 5 + exception);
								}
								pthread_rwlock_unlock(&errorptr);
								//添加到队列尾
								pthread_rwlock_wrlock(&retptr);
								if(ret_head == ret_tail)
								{
									ret_head->next = ret_p1;

								}else{
								
									ret_tail->next = ret_p1;
								}
								ret_tail = ret_p1;
								pthread_rwlock_unlock(&retptr);
								//注册写事件
								ev.data.fd = events[i].data.fd;
								ev.events = EPOLLOUT | EPOLLET;
								epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev);
								printf("listen output event\n");
							}
							else if(*cmd == 0x0F)
							{
								printf("to set all key\n");
								//设置标志置为1，添加task节点至队尾
								task_p1 = (task *)malloc(sizeof(task));
								task_p1->fd = events[i].data.fd;
								task_p1->location = 0x00;
								task_p1->next = NULL;
								memcpy(task_p1->data, cmd + 5, cmd_length - 7);
								pthread_rwlock_wrlock(&taskptr);
								if(task_head == task_tail)
								{
								  task_head->next = task_p1;
								  printf("only one\n");
								}
								else
								  task_tail->next = task_p1;
								task_tail = task_p1;
								to_set_key = 1;	
								pthread_rwlock_unlock(&taskptr);
								printf("add task\n");

							}
							else if(*cmd == 0x05)
							{
								printf("to set one\n");
								//设置标志置为1，添加task节点到队尾
								task_p1 = (task *)malloc(sizeof(task));
								task_p1->fd = events[i].data.fd;
								task_p1->location = cmd[2];
								task_p1->data[0] = cmd[3];
								task_p1->next = NULL;
								pthread_rwlock_wrlock(&taskptr);
								if(task_head == task_tail)
								  task_head->next = task_p1;
								else
								  task_tail->next = task_p1;
								task_tail = task_p1;
								to_set_key = 1;	
								pthread_rwlock_unlock(&taskptr);
								printf("add task\n");
							}
							else if(*cmd == 0x07)
							{
								printf("to set addr information\n");
								//删除全部任务
								printf("wait for task complete...\n");
								while(pthread_rwlock_trywrlock(&taskptr) == EBUSY);
								printf("delete unprocessed task...\n");
								pthread_rwlock_wrlock(&taskptr);
								for(task_p1 = task_head->next; task_p1 != NULL; task_p1 = task_head->next)
								{
									task_head->next = task_p1->next;
									printf("before free task_p1\n");
									free(task_p1);
									task_p1 = NULL;
								}
								to_set_key = 0;
								task_tail = task_head;
								pthread_rwlock_unlock(&taskptr);
								//等待扫描结束
								printf("wait for search complete...\n");
								while(pthread_mutex_trylock(&searchptr) == EBUSY);
								//清空异常节点
								printf("clear exception...\n");
								pthread_rwlock_wrlock(&errorptr);
								exception = 0;
								memset(erroraddr, 0, 254);
								pthread_rwlock_unlock(&errorptr);
								printf("save address...\n");
								//保存板卡信息
								pthread_rwlock_wrlock(&addressptr);
								bzero(address, sizeof(address));
								board_num = cmd[2];
								status_length = (board_num % 8) ? (board_num / 8 + 1) : (board_num / 8);
								save_addr(&cmd[3], board_num, cmd_length);
								pthread_rwlock_unlock(&addressptr);
								printf("board_num: %d, addr: ",board_num);
								for(j = 0; j < board_num; j++)
								{
									printf("%d, ", address[j]);
								}
								printf("status size: %d\n", status_length);
								//申请板卡状态存储区
								printf("allocate memery for status...\n");
								pthread_rwlock_wrlock(&statusptr);
							//	if(status == NULL)
							//	{
							//	  status = (unsigned char *)malloc(status_length * sizeof(unsigned char));
							//	}
							//	else
								if(status != NULL)
								{
									printf("before free status %d\n", status);
									printf("status: ");
									for(t = 0; t < status_length; t++)
									  printf("%2x ", status[t]);
									printf("\n");
									free(status);
									status = NULL;
									printf("ok\n");
								}
								status = (unsigned char *)malloc(status_length * sizeof(unsigned char));
								if(status == NULL)
								{
									printf("allocate memery for status failed\n");
									return -1;
								}
								printf("allocate memery for status sucess\n");
								memset(status, 0, status_length);
								pthread_rwlock_unlock(&statusptr);
								init = 1;
								pthread_mutex_unlock(&searchptr);
								//保存初始化文件
								save_init(cmd[2], &cmd[3], cmd_length - 5);
								//添加返回节点
								ret_p1 = (ret_link *)malloc(sizeof(ret_link));
								ret_p1->sockfd = events[i].data.fd;
								//构建返回值
								memcpy(ret_p1->ret, cmd, cmd_length);
								//转换成字符形式
								itoc(ret_p1->ret, cmd_length);
								ret_p1->next = NULL;
								//添加到队列尾
								pthread_rwlock_wrlock(&retptr);
								ret_tail->next = ret_p1;
								ret_tail = ret_p1;
								pthread_rwlock_unlock(&retptr);
								printf("return point has added\n");
								//注册写事件
								ev.data.fd = events[i].data.fd;
								ev.events = EPOLLOUT | EPOLLET;
								if(epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
									printf("modifile event fail\n");
								else
								  	printf("listen output event\n");
							}

						}
						break;
					case -1:
						printf("get net data error\n");
						bzero(cmd_temp, sizeof(cmd_temp));
						break;
					default:
						printf("wrong opt\n");
				}

			}
			else if(events[i].events & EPOLLOUT)
			{
				if(send_net_data(events[i].data.fd) == 0)
				  printf("already send data to client\n");
				else
				  printf("the client has no task!\n");
				//修改为读事件
				ev.data.fd = events[i].data.fd;
				ev.events = EPOLLIN | EPOLLET;
				if(epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
				  	printf("modify event fail\n");
				else
					printf("listen input event\n");
			}
		}
	}
}

//功能：保存板卡地址，
//入口：板卡地址存储数组，板卡数，命令长度
//返回值：无
void save_addr(unsigned char *addr, int n , int length)
{
	unsigned char *addrp = address;
	int i = 0, limit = length - 5;//地址域长度
	while(n > 0)
	{
		if((addr[limit - (i / 8) - 1] >> (i % 8)) & 1)
		{
			*addrp = i + 1;
			addrp++;
			n--;
		}
		i++;
	}
}
//功能：获取网络数据，并进行命令解析
//入口：套接字描述符, 用于存储命令的字符串
//出口：要完成的命令类型 option

int get_net_data(const int socked, unsigned char *cmd_temp)
{
	int i, n = 0, j = 2, total = 0, byte = 1023;		//命令类型
	unsigned char recv[1024];//读网络数据的缓冲区
	unsigned char char_num;  //有效命令字节数 
	unsigned char *ptr = recv;
	//清空命令缓冲
	bzero(cmd_temp, sizeof(cmd_temp));
	bzero(recv, sizeof(recv));
	errno = 0;
	//读数据
	while(((n = read(socked, ptr, byte)) > 0) && (byte > 0))
	{
		ptr += n;
		byte -= n;
		total += n;
		errno = 0;
	}
	recv[total] = '\0';
	printf("received %d data: %s\n",total, recv);
	//判断结束状态
	if(n < 0)
	{
		if(errno == ECONNRESET)
		{
			//掉线
			printf("ECONNRESET\n");
			return to_close;
		}
		else if(errno == EWOULDBLOCK)
		{
			//读完数据,截取命令
			for(i = total - 1; i >= 0; i--)
			{
				//从后向前寻找报头的'：'
				if(recv[i] != ':')
				  continue;
				else
				{
					//判断是否为完整的命令
					ctoi(&recv[i + 1], &char_num, &j);
					if(strlen(&recv[i+3]) != char_num)
					{
						//命令不完整，重新搜索：
						recv[i] = '\0';
						continue;

					}else{
						//命令完整，存入cmd_temp
						memcpy(cmd_temp, &recv[i+3], char_num);
						cmd_temp[char_num] = '\0';
						bzero(recv, sizeof(recv));
						break;
					}
					
				}
			}
			if(i < 0)
			{
				//接收到错误命令
				return -1;
			}
			else
			  return sucess;
		}
		else{
			
			//错误
			perror("read from net failed");
			return -1;
		}
	}else if(n == 0)
	{
		return to_close;
	}
	else{
		
		//用户缓冲区满的处理以后加上
		printf("user buffer fulfilled\n");
		return -1;
	}
}

//功能：将字符型转换成整型
//入口：源字符串 ,目的字符串， 转换长度
//出口：转换后的整型字串,以\0结尾
void ctoi(unsigned char *src, unsigned char *des, int* length)
{
	int i, j;
	for(i = 0; i < *length; i = i+2 )
	{
		for(j = 0; j < 2; j++)
		{
			if(src[i + j] > 64)
		  		src[i + j] -= 55;
			else
				src[i + j] -= 48;
		}
		des[i / 2] = src[i] * 16 + src[i + 1];
	}
	if(*length > 2)
	  des[*length / 2] = '\0';
	*length = *length / 2;
//	for(i = 0; i < *length; i++)
//	  printf("cmdi: %2x ", des[i]);
//	printf("\n");
}

//功能：将整型转换成字符型,\0结尾
//入口：unsigned数组，存储整型值
//出口：unsigned数组，存储字符型值
void itoc(unsigned char *ptr, int length)
{
	unsigned char y[length], a;
	int i;
	memcpy(y, ptr, length);
	bzero(ptr, length);
	for(i = 0; i < (length * 2); i++)
	{
		if((i % 2) == 0)
		  a = (y[i / 2] >> 4) & 0x0F;
		else
		  a= y[i / 2] &0x0F;
		if(a > 9)
		  a += 55;
		else
		  a += 48;
		ptr[i] = a;
	}
	ptr[length * 2] = '\0';
}

//CRC校验
//获得CRC校验
//参数：校验数据首地址，校验数据长度
//返回值：CRC校验位
uint getCRC16(unsigned char *puchMsg , uint usDataLen) //*puchMsg 为待校验数据首地址，usDataLen为待校验数据长度
{
	unsigned char CRCH = 0xFF ; // 高CRC字节初始化 
	unsigned char CRCL = 0xFF ; // 低CRC 字节初始化
	unsigned char uIndex ;      // CRC循环中的索引 
    
	while (usDataLen--) // 传输消息缓冲区 
	{
		uIndex = CRCH ^ *puchMsg++;// 计算CRC
		CRCH = CRCL ^ auchCRCHi [uIndex] ;
		CRCL = auchCRCLo [uIndex] ;
	}
	return (CRCH<< 8 | CRCL);
}

//功能：向客户端发数据
//入口：套接字描述符
int send_net_data(int fd)
{
	ret_link *ptr, *p = ret_head;
	pthread_rwlock_wrlock(&retptr);
	for(ptr = ret_head->next ; ptr != NULL; ptr = ptr->next)
	{
		printf("find return point\n");
		//找到套接字对应的返回链表节点
		if(ptr->sockfd != fd)
		{
			p = ptr;
		  	continue;
		}
		else{
			//向客户端发数据
			errno = 0;
			printf("send data to the client %d: %s\n", fd, ptr->ret);
			if(write(fd, ptr->ret, strlen(ptr->ret)) == -1) 
			  perror("send data to client failed");
			//删除链表节点
			p->next = ptr->next;
			printf("before free return\n");
			free(ptr);
			ptr = NULL;
			printf("remove return point\n");
			if(p->next == NULL)
			  ret_tail = p;
			pthread_rwlock_unlock(&retptr);
			return 0;
		}
	}
	pthread_rwlock_unlock(&retptr);
	return -1;
}
