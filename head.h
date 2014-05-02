//返回值队列
typedef struct RETURN_LINK{
	
	int sockfd;   //套接字描述符
	unsigned char ret[100];  //命令响应字符串，用于网络通信
	struct RETURN_LINK *next;  //指向返回链表的下一项

} ret_link;
//任务队列
typedef struct TASK_LINK{

	int fd;    //套接字描述符
	unsigned char location;
	unsigned char data[100];  //命令及设置值
	struct TASK_LINK *next;
} task;
//CRC表

int read_init(int* board_num, unsigned char *address_flag);
int save_init(unsigned char board_num, unsigned char* addr_i, int length);
int set_non_blocking(int sock);
int get_net_data(const int socked, unsigned char *cmd_temp);
void ctoi(unsigned char *src, unsigned char *des, int *length);
void itoc(unsigned char *ptr, int length);
uint getCRC16(unsigned char *puchMsg , uint usDataLen);
int send_net_data(int fd);
void seri_init();
unsigned char get_sum(const unsigned char *str);
void* comm_seri(void *arg);
int communicate(unsigned char *send_buf, unsigned char *returnstring);
void save_addr(unsigned char *addr, int n , int length);
int get_board_toset(unsigned char *data_tobe, unsigned char *to_set, unsigned char *to_be);
