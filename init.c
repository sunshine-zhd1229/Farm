#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <fcntl.h>
#include  <math.h>
#include    "head.h"
int read_init(int* board_num, unsigned char *address_flag)
{
	FILE* fd;
	char buf[1024], *p, num_temp[3];
	int i = 0, n = 0;
	if((fd = fopen("/project/farm/file.ini", "r")) == NULL)
	{ 
		perror("open initial file fail: ");
		return -1;
	}
	bzero(buf, sizeof(buf));
	//读板卡个数
	while((p = fgets(buf, 1024, fd)))
	{
		if(strncmp("board_num = ", buf, 12) == 0)
		  break;
	}
	if( p == NULL)
	{
		printf("get board_num fail\n");
		fclose(fd);
		return -1;
	}
	//去掉换行符
	buf[strlen(buf) - 1] = '\0';
	for(i = 0; *p != '='; p++, i++);
	i += 2;
	n = strlen(&buf[i]);
	strncpy(num_temp, &buf[i], n);
	for(i = 0; i < n; i++)
	{
		if(num_temp[i] < 65)
			num_temp[i] -= 48;
		else
		  	num_temp[i] -= 55;
		*board_num += num_temp[i] * pow(16.0, (n - 1 - i));  
	}
	printf("board_num = %d, address: ", *board_num);
	//读板卡地址
	while((p = fgets(buf, 1024, fd)))
	{
		if(strncmp("address = ", buf, 10) == 0)
		  break;
	}
	if(p == NULL)
	{
		printf("get address fail\n");
		fclose(fd);
		return -1;
	}
	//去掉换行符
	buf[strlen(buf) - 1] = '\0';
	for(i = 0; *p != '='; p++, i++);
	i += 2;
	n = strlen(&buf[i]);
	strncpy(address_flag, &buf[i], n);
	address_flag[n] = '\0';
	fclose(fd);
	return 0;
}

int save_init(unsigned char num, unsigned char* addr_i, int length)
{
	unsigned char board[3], addr_c[length * 2 + 1];
	unsigned char buf[70];
	FILE *fd;
	board[0] = num;
	memcpy(addr_c, addr_i, length); //non-null terminated
	if((fd = fopen("/project/farm/file.ini", "w")) == NULL)
	{ 
		perror("open initial file fail: ");
		return -1;
	}
	itoc(board, 1);//null-terminated
	printf("init: %s\n",board);
	memset(buf, 0, 70);
	strncpy(buf,"board_num = ", 12);
	strncat(buf + 12, board, 2);
	buf[14] = '\n';
	buf[15] = '\0';
	fputs(buf, fd);
	fputs(buf, stdout);
	itoc(addr_c, length); //null_terminated
	printf("%s\n", addr_c);
	memset(buf, 0, 70);
	strncpy(buf, "address = ", 10);
	strncat(buf + 10, addr_c, length * 2);
	buf[10 + length * 2] = '\n';
	buf[10 + length * 2 + 1] = '\0';
	fputs(buf, fd);
	fputs(buf, stdout);
	printf("save init file\n");
	fclose(fd);
}
