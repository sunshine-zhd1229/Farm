#include  <stdio.h>
#include  <unistd.h>
#include  <errno.h>
#include  <pthread.h>
#include  <sys/types.h>
#include  <stdlib.h>
#include  <string.h>
#include  <termios.h>
#include  <fcntl.h>
#include  <sys/epoll.h>
#include  <sys/time.h>
#include    "head.h"

#define		databits	8
#define     stopbits	1
#define     name_num	3
#define     speed_num	3
#define     key_mask	0

extern int errno;
//队列
extern ret_link *ret_head; 
extern ret_link *ret_tail;
extern task *task_head;
extern task *task_tail;

//读写锁
extern pthread_rwlock_t taskptr; 
extern pthread_rwlock_t retptr;
extern pthread_rwlock_t statusptr;
extern pthread_rwlock_t addressptr;
extern pthread_rwlock_t errorptr;
extern pthread_mutex_t searchptr;

//设置继电器标志
extern int to_set_key;

//其他各种全局变量
extern unsigned char *status;   //板卡状态
extern unsigned char address[254];  //地址
extern unsigned char erroraddr[254]; //异常标记
extern int exception;	//异常个数
extern int status_length; //status长度
extern int board_num; //板卡个数
extern int epfd;  //事件描述符
extern int init; //初始化标志
//串口文件描述符	
int seri_fd = -1; 

//串口线程函数
void* comm_seri(void *arg)
{
	int i, j, k, set_num, fail = 0, index = 0;// 
	struct epoll_event evt; //用于注册事件
	task *task_p;	  //用于扫描队列
	ret_link *ret_p;
	uint CRCDATA;
	//时间相关变量
	struct timeval tvafter, tvpre;
	struct timezone tz;
	int interval = 0;
	//命令缓冲
	unsigned char s_buf[9] = {0x00, 0x5A, 0x51}; 
	//返回值缓冲区
	unsigned char r_buf[9];
	unsigned char temp[32];
	unsigned char to_set[254], to_be[254], fail_addr[254], err_temp[254];
	printf("come in serial thread\n");
	gettimeofday(&tvpre, &tz);
	//串口初始化
	seri_init();
	while(1)
	{
		//设置继电器
		if(to_set_key == 1)
		{
			fail = 0;
			s_buf[5] = 0x00;
			s_buf[6] = 0x00;
			s_buf[7] = 0x00;
			//从任务列表取出第一个任务
			pthread_rwlock_wrlock(&taskptr);
			task_p = task_head->next;
			//判断任务类型
			if(task_p->location == 0x00)
			{	
				printf("start to set all\n");
				//设置所有节点
				memset(fail_addr, 0, 254);
				pthread_rwlock_rdlock(&statusptr);
				memcpy(temp, status, status_length);
				pthread_rwlock_unlock(&statusptr);
				//设置地址和命令
				for(i = 0; i < board_num; i++)
				{
					pthread_rwlock_rdlock(&addressptr);
					s_buf[3] = address[i];
					pthread_rwlock_unlock(&addressptr);
					k = status_length - ((s_buf[3] - 1) / 8 ) - 1;
					if(((task_p->data[k] >> ((s_buf[3] - 1) % 8)) & 0x01) == 1)
					  	s_buf[4] = 0x01;
					else
					 	s_buf[4] = 0x02;
					s_buf[8] = get_sum(s_buf);
					//串口发送命令
					if(communicate(s_buf, r_buf) == -1)
					{
						printf("set key %d failed\n", s_buf[3]);
						fail_addr[fail] = s_buf[3];
						pthread_rwlock_wrlock(&errorptr);
						if(erroraddr[s_buf[3] - 1] == 0)
						{
							erroraddr[s_buf[3] - 1] = 1;
							exception++;
						}
						pthread_rwlock_unlock(&errorptr);
						fail++;
					}
					else{

						//设置成功则查询开关状态
						s_buf[4] = 0x07;
						s_buf[8] = get_sum(s_buf);
						if(communicate(s_buf, r_buf) == -1)
						{
							fail_addr[fail] = s_buf[3];
							pthread_rwlock_wrlock(&errorptr);
							if(erroraddr[s_buf[3] - 1] == 0)
							{
								erroraddr[s_buf[3] - 1] = 1;
								exception++;
							}
							pthread_rwlock_unlock(&errorptr);
							fail++;
						}
						else{
							//查询状态成功,更新状态
							r_buf[6] = (~((r_buf[6] & 0x01) ^ key_mask) & 0x01);  //取开关状态
							if(r_buf[6] == 1)
							{
								temp[status_length - 1 - ((s_buf[3] - 1) / 8)] |= (0x01 << ((s_buf[3] - 1) % 8)); 	
							}else{
								temp[status_length - 1 - ((s_buf[3] - 1) / 8)] &= ~(0x01 << ((s_buf[3] - 1) % 8)); 	
							}
							pthread_rwlock_wrlock(&errorptr);
							if(erroraddr[s_buf[3] - 1] == 1)
							{
								erroraddr[s_buf[3] - 1] = 0;
								exception--;
							}
							pthread_rwlock_unlock(&errorptr);
						}
					}

				}
				//更新状态
				pthread_rwlock_wrlock(&statusptr);
				memcpy(status, temp, status_length);
				pthread_rwlock_unlock(&statusptr);
				//添加返回链表项
				ret_p = (ret_link *)malloc(sizeof(ret_link));
				ret_p->sockfd = task_p->fd;
				ret_p->next = NULL;
				//构建返回值
				ret_p->ret[0] = 0X0F;
				ret_p->ret[1] = 0x00;
				ret_p->ret[3] = 0x00;
				if(fail != 0)
				{
					ret_p->ret[2] = 0xFF;
					ret_p->ret[4] = fail;
					memcpy(&ret_p->ret[5], temp, status_length);
					memcpy(&ret_p->ret[5 + status_length], fail_addr, fail);
					CRCDATA = getCRC16(ret_p->ret, 5 + fail + status_length);
					ret_p->ret[5 + fail + status_length] = (CRCDATA >> 8) & 0xFF;
					ret_p->ret[6 + fail + status_length] = CRCDATA & 0xFF;
					itoc(ret_p->ret, 7 + fail + status_length);
					fail = 0;
				}
				else
				{
					pthread_rwlock_rdlock(&addressptr);
					ret_p->ret[2] = address[0];
					pthread_rwlock_unlock(&addressptr);  	ret_p->ret[4] = board_num;
					memcpy(&ret_p->ret[5], temp, status_length);
					CRCDATA = getCRC16(ret_p->ret, 5 + status_length);
					ret_p->ret[5 + status_length] = (CRCDATA >> 8) & 0xFF;
					ret_p->ret[6 + status_length] = CRCDATA & 0xFF;
					itoc(ret_p->ret, 7 + status_length);
				}
				printf("0F return: %s\n", ret_p->ret);
			}
			else{
				
				//设置单个节点
				printf("start to set one\n");
				s_buf[3] = task_p->location;
				if(task_p->data[0] == 0x01)
				  s_buf[4] = 0x01;
				else
				  s_buf[4] = 0x02;
				s_buf[8] = get_sum(s_buf);
				if(communicate(s_buf, r_buf) == -1)
				{
					printf("set key %d failed\n", s_buf[3]);
					fail++;	
				}
				else{
					//查询开关状态
					s_buf[4] = 0x07;
					s_buf[8] = get_sum(s_buf);
					if(communicate(s_buf, r_buf) == -1)
					{
						printf("get key %d status failed\n", s_buf[3]);
						fail++;
					}
					else{
						r_buf[6] = (~((r_buf[6] & 0x01) ^ key_mask) & 0x01);  //取开关状态
						//设置成功,更新状态
						pthread_rwlock_rdlock(&addressptr);
						for(j = 0; j < board_num; j++)
						{
							if(address[j] == task_p->location)
							  break;
						}
						pthread_rwlock_unlock(&addressptr);
						pthread_rwlock_wrlock(&statusptr);
						if(r_buf[6] == 1)
							status[status_length - 1 - (j / 8)] |= (0x01 << (j % 8));
						else
							status[status_length - 1 - (j / 8)] &= ~(0x01 << (j % 8));
						pthread_rwlock_unlock(&statusptr);
						//printf("%2x %2x\n", status[0], status[1]);
					}
				}
				//添加返回链表项
				ret_p = (ret_link *)malloc(sizeof(ret_link));
				ret_p->sockfd = task_p->fd;
				ret_p->next = NULL;
				//构建返回值
				ret_p->ret[0] = 0X05;
				ret_p->ret[1] = 0x00;
				ret_p->ret[2] = task_p->location;
				if(fail != 0)
				{
					ret_p->ret[3] = 0x0F;		
					pthread_rwlock_wrlock(&errorptr);
					if(erroraddr[task_p->location - 1] == 0)
					{
						erroraddr[task_p->location - 1] = 1;
						exception++;
						
					}
					pthread_rwlock_unlock(&errorptr);
					
				}
				else
				{
				  	ret_p->ret[3] = r_buf[6];
					pthread_rwlock_wrlock(&errorptr);
					if(erroraddr[task_p->location - 1] == 1)
					{
						erroraddr[task_p->location - 1] = 0;
						exception--;
					}
					pthread_rwlock_unlock(&errorptr);
				}
				CRCDATA = getCRC16(ret_p->ret, 4);
				ret_p->ret[4] = (CRCDATA >> 8) & 0xFF;
				ret_p->ret[5] = CRCDATA & 0xFF;
				itoc(ret_p->ret, 6);
			}
			//添加到队列尾	
			printf("set key done\n");
			pthread_rwlock_wrlock(&retptr);
			ret_tail->next = ret_p;
			ret_tail = ret_p;
			pthread_rwlock_unlock(&retptr);
			//注册写事件
			evt.data.fd = task_p->fd;
			evt.events = EPOLLOUT | EPOLLET;
			epoll_ctl(epfd, EPOLL_CTL_MOD, evt.data.fd, &evt);
			printf("listen output event\n");
			//删除任务队列项
			if(task_p == task_tail)
			{
				task_tail = task_head;
				task_head->next = NULL;
				printf("no task\n");
				to_set_key = 0;
			}else{
				task_head->next = task_p->next;
			}

			free(task_p);
			task_p = NULL;
			pthread_rwlock_unlock(&taskptr);		
			printf("move task\n");
		}
		else{
		//读状态
			gettimeofday(&tvafter, &tz);
			interval = (tvafter.tv_sec - tvpre.tv_sec) + ((tvafter.tv_usec - tvpre.tv_usec) / 1000000);
			if(init == 1 || interval >= 60)
			{
				pthread_mutex_lock(&searchptr);
				//读串口状态
				fail = 0;
				memcpy(temp, status, status_length);
				memset(err_temp, 0, sizeof(err_temp));
				s_buf[4] = 0x07;
				s_buf[5] = 0x00;
				s_buf[6] = 0x00;
				s_buf[7] = 0x00;
				for(i = board_num - 1; i >= 0; i--)
				{
					s_buf[3] = address[i];
					s_buf[8] = get_sum(s_buf);
					//调用通信函数
					if(communicate(s_buf, r_buf) == -1)
					{
						printf("get key status of board %d failed\n", s_buf[3]);
						err_temp[s_buf[3] - 1] = 1;
						fail++;

					}else{
						//存入temp
						r_buf[6] = ((~((r_buf[6] & 0x01) ^ key_mask)) & 0x01);  //取开关状态,实际状态与mask的异或
						index = status_length - 1 - (i / 8);
						if(r_buf[6] == 1)
							temp[index] |= (1 << (i % 8));  //存开关状态
						else
						  	temp[index] &= ~(1 << (i % 8));
						usleep(10000);
					}
				}
				memset(erroraddr, 0, 254);
				exception = fail;
				memcpy(erroraddr, err_temp, 254);
				memcpy(status, temp, status_length);
				init = 0;
				pthread_mutex_unlock(&searchptr);
				memcpy(&tvpre, &tvafter, sizeof(struct timeval));
			}
		}
		
	}
}
void seri_init()
{
	//串口设备名
	char *serial_name[4] = {"/dev/ttySAC0", "/dev/ttySAC1", "/dev/ttySAC2", "/dev/ttySAC3"};
	//波特率
	speed_t	speed_arr[8] = {B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300};
	//串口结构体
	struct termios opt;
	int reset = 1;		//串口重设标志
	//打开串口:读写，不为串口控制终端，阻塞
	while((seri_fd = open(serial_name[name_num], O_RDWR | O_NOCTTY)) == -1)
	{
		printf("open serial port %s wrong: %s\n", serial_name[name_num], strerror(errno));
	}

	//获取串口属性	
	while(tcgetattr(seri_fd, &opt) == -1)
	{
		printf("get serial port attribute wrong: %s\n", strerror(errno));
	}

	while(reset == 1)
	{
		//设置串口为非规范模式
		opt.c_cflag |= (CLOCAL | CREAD); //忽略modem状态线，读入使能
		opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG | IEXTEN); //设置串口为非规范模式，禁用字符回射
		opt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
		opt.c_oflag &= ~OPOST; //不对输出进行特殊字符处理
		opt.c_cc[VTIME] = 1;   //设置read timer为100ms
		opt.c_cc[VMIN] = 0;    //从read调用开始100ms未响应，则返回

		//设置数据位
		opt.c_cflag &= ~CSIZE;
		switch ( databits )
		{
			case 8 :
				opt.c_cflag |= CS8;
				break;
			case 7 :
				opt.c_cflag |= CS7;		
				break;
			case 6 :
				opt.c_cflag |= CS6;
				break;
			case 5 :
				opt.c_cflag |= CS5;
				break;
			default :
				printf("wrong datebits!\n");
				break;
		}

		//设置停止位
		switch ( stopbits )
		{
			case 1 :
				opt.c_cflag &= ~CSTOPB;
				break;
			case 2 :
				opt.c_cflag |= CSTOPB;
				break;
			default :
				printf("wrong stopbits!\n");
				break;
		}

		//取消校验
		opt.c_cflag &= ~PARENB;
		//设置波特率
		cfsetispeed(&opt, speed_arr[speed_num]);
		cfsetospeed(&opt, speed_arr[speed_num]);
		//更新设置
		if(tcsetattr(seri_fd, TCSANOW, &opt) == -1)
		{
			printf("set serial port attribute wrong:%s\n", strerror(errno));
			reset = 1;
		}else{
			
			printf("serial port initial successed\n");
			reset = 0;
		}
	}
}
unsigned char get_sum(const unsigned char *str)
{
	unsigned char sum = 0x00;
	int i;
	for(i = 0; i < 8; i++)
	{
		sum += str[i];
	}
	return sum;
}
int communicate(unsigned char *send_buf, unsigned char *returnstring)
{
	int n = 9, nbyte, error = 0;
	unsigned char *ptr = send_buf;
	unsigned char sum;
	int writeflag = 0, i;
	int readflag = 0;
	int timeout_count = 0;
	bzero(returnstring, 9);
//	printf("check status\n");	
//	printf("seri_cmd: ");
//	for(i = 0; i < 9; i++)
//	{
//		printf("%2x ", send_buf[i]);
//	}
	while((error == 0) && (readflag == 0))
	{
		
		//写命令
		if(writeflag == 0)
		{
			while(n > 0)
			{
				nbyte = write(seri_fd, ptr, n);
				if(nbyte == -1)
				{
					printf("serial port cmd write wrong: %s\n", strerror(errno));
					error = 1;
				}else{
	
					n -= nbyte;
					ptr += nbyte;
				}
			}
//			printf("write cmd done\n");
			writeflag = 1;
			n = 9;
			ptr = returnstring;
		}
		//读数据
		while((n > 0) && (writeflag == 1))
		{
			nbyte = read(seri_fd, ptr, n);
//			printf("get %d data\n", nbyte);
			switch( nbyte )
			{
				//读取失败,退出函数
				case -1:
					printf("serial port response fail: %s\n", strerror(errno));
					error = 1;
					writeflag = 0;
					break;
				//超时,重发命令;10次超时，重启串口
				case 0:
					timeout_count++;
					writeflag = 0;
					if(timeout_count == 3)
					{

						//重启串口
						printf("serial port communication wrong!\n");
						while((tcflush(seri_fd, TCIOFLUSH) == -1));
						timeout_count = 0;
						error = 1;
	
					}else{

						//重发命令
						printf("serial port read timeout: %s, send cmd angin\n", strerror(errno));
						memset(returnstring, 0, 9);
						n = 9;
						ptr = send_buf;
						writeflag = 0;
					}
					break;
					//读取成功
				default:
					n -= nbyte;
					ptr += nbyte;
			}
		}
		if( n == 0)
			readflag = 1;
		if(readflag == 1)
		{
			sum = get_sum(returnstring);
			if(sum != returnstring[8])
			{
				//校验错误，重新发送命令
				printf("sum check wrong,send cmd again\n");
				while((tcflush(seri_fd, TCIOFLUSH) == -1));
				writeflag = 0;
				readflag = 0;
				n = 9;
				ptr = send_buf;
			}else{
//				printf("got status\n");
			}
		}
	}

	if(error == 1)
	  return -1;
	else
	  return 0;
}

int get_board_toset(unsigned char *data_tobe, unsigned char *to_set, unsigned char *to_be)
{
	int i, j, n = 0, k = 0;
	unsigned char key;
	for(i = status_length - 1; i >= 0; i--)
	{
		key = status[i] ^ data_tobe[i];
		for(j = 0; (j < 8) && (k * 8 + j < board_num) ; j++)
		{
			if((key >> j) & 1)
			{
				pthread_rwlock_rdlock(&addressptr);
				to_set[n] = address[k * 8 + j];
				pthread_rwlock_unlock(&addressptr);
				to_be[n] = (data_tobe[i] >> j) & 0x01;
				n++;
			}
		}
		k++;
	}
	return n;
}
