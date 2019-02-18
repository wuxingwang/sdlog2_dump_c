/****************************************************************************
 *
 *   Copyright (c) wuxingwang . All rights reserved.
 *
 *  The copyright of software is protected by law.It is forbidden to use, disseminate,
 *  and publish on the Internet without the consent of the author. The offender will
 *  pursue legal liability.Any copy modification must retain this copyright notice information
 *  and original author information
 *
 *  软件版权受法律保护,未经作者同意禁止使用,传播,发布于互联网等,违者将追究法律责任,
 *  任何复制修改必须保留本版权声明信息和原作者信息
 *
 * @file sdlog2_dump.c
 * convert  PX4	sdlog2 log to csv files
 *
 * @author 吴兴旺 wuxingwang <chinawuxingwang@163.com>
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "sdlog2_format.h"


#define MAX_MSG_ITEM 50

#define LOG_VER_MSG 130
#define LOG_INFO_MSG 135//Custom type, unofficial PX4

#define NEW_LINE_MODE 2//1.根据时间戳写入一行，2.判断一行是否有新数据写入一行

/*下面两处结构体也可以通过动态解析*/
/* --- VER - VERSION --- */
#define LOG_VER_MSG 130
struct log_VER_s {
	char arch[16];
	char fw_git[64];
};


/* --- INFO - device info version, sn etc. --- */
#define LOG_INFO_MSG 135
struct log_INFO_s {
	char fc_uid[16];
	char fc_ver[16];
	char amu_ver[16];
	char geo_ver[16];
	char res[16];
	char res2[16];
	uint16_t uav_type;    //飞机类型，决定喷洒方式
	uint16_t system_type; //机架类型
	uint32_t ability;     //能力集
};

struct msg_format_s{

    struct log_format_s body;
    void * p_data;
    int struct_number;
    int not_write_csv;   //if 1, not write to csv,for version, UID etc. message,just printf
    int data_not_updated;//Data has  been not writed to CSV file

};

struct msg_format_s msg_table[MAX_MSG_ITEM];
int valid_msg_cnt = 0;

int parse_format_msg(FILE *p_file)
{
	uint8_t head1,head2,msg_type = 0; 
	int idx = 0;
	
	int struct_number = 0;
	char name[4+1];
	char format[16+1];
	char labels[64+1];
	int ret = 0;
	
	while(1)
	{
		head1 = 0;
		head2 = 0;
		msg_type = 0;
		
		ret = fread(&head1, 1, 1, p_file);
		ret += fread(&head2, 1, 1, p_file);
		ret += fread(&msg_type, 1, 1, p_file);
		
		if (HEAD_BYTE1 != head1 || HEAD_BYTE2 != head2 || ret != 3)
		{
			printf("find format error 0x%x, 0x%x\n", head1, head2);
			return -1;
		}

		if (LOG_FORMAT_MSG != msg_type)
		{
			printf("find format end\n");
			fseek(p_file, -3, SEEK_CUR);
			break;
		}
		
		if ( fread((void*)&msg_table[idx].body, sizeof(struct log_format_s), 1, p_file)
		    != 1 )
		{
			printf("fread log_format_s error\n");
			return -1;
		}
		/*计算结构体成员的数量，后面用到*/
		struct_number = 0;
		while(msg_table[idx].body.format[struct_number] !=0 && struct_number < sizeof(msg_table[idx].body.format))
		{
			struct_number++;
		}
		msg_table[idx].struct_number = struct_number;
		/*分配内存*/
		msg_table[idx].p_data = malloc(msg_table[idx].body.length-LOG_PACKET_HEADER_LEN);
		memset(msg_table[idx].p_data, 0, msg_table[idx].body.length-LOG_PACKET_HEADER_LEN);
		
		strncpy(name, msg_table[idx].body.name,sizeof(name)-1);
		name[sizeof(name)-1] = 0;
		strncpy(format, msg_table[idx].body.format,sizeof(format)-1);
		format[sizeof(format)-1] = 0;
		strncpy(labels, msg_table[idx].body.labels,sizeof(labels)-1);
		labels[sizeof(labels)-1] = 0;
		//printf("%02d --> %s, %s, %d, %d\n",idx, name, format, msg_table[idx].body.length, struct_number);
		msg_table[idx].data_not_updated = 0;

		if (msg_table[idx].body.type == LOG_VER_MSG || msg_table[idx].body.type == LOG_INFO_MSG)
		{
			msg_table[idx].not_write_csv = 1;
		}
		else
		{
			msg_table[idx].not_write_csv = 0;
		}

		idx++;
		valid_msg_cnt = idx;
		if (idx >= MAX_MSG_ITEM)
		{
			printf("msg item exceed %d\n",MAX_MSG_ITEM);
			return -1;
		}
	}
	
	return 0;
}

void free_mem(void)
{
	int i;

	for (i = 0; i < MAX_MSG_ITEM; i++)
	{
		if (msg_table[i].p_data != NULL )
		{
			free(msg_table[i].p_data);
			msg_table[i].p_data = NULL;
		}

	}

}
int write_column_tile(FILE *p_csv_file)
{
	int i=0,j=0;
	
	char item[32];
	char name[4+1];
	char labels[64+1];
	char *p = NULL;
	char *token = NULL;
	
	memset(name,0,sizeof(name));
	memset(labels,0,sizeof(labels));
	for( i = 0; i < valid_msg_cnt; i++)
	{

		strncpy(name, msg_table[i].body.name, 4);
		name[sizeof(name)-1] = '\0';
		strncpy(labels, msg_table[i].body.labels, 64);
		labels[sizeof(labels)-1] = '\0';

		j = 0;
		p = labels;
		while((token = strsep(&p, ",")) != NULL)
		{
			if(msg_table[i].body.format[j] == 0)
			{
				printf("labes and format is not Corresponding\n");
				return -1;
			}
			snprintf(item,sizeof(item), "%s_%s", name, token);
			//printf("%s\n", item);
			fwrite(item, 1, strlen(item), p_csv_file);
			if (i == (valid_msg_cnt - 1) && j == (msg_table[i].struct_number - 1))
			{
				fwrite("\r\n",1,2,p_csv_file);
			}
			else
			{
				fwrite(",",1,1,p_csv_file);
			}
			j++;
			
		}
		

	}
	
	return 0;
}

int find_msg_begain(FILE * p_file)
{
	uint8_t head[2];

	
	if (fread(head, 1, 2, p_file) !=  2)
	{
		printf("find_msg_begain file end\n");
		return -1;
	}
	if (head[0] == HEAD_BYTE1 && head[1] == HEAD_BYTE2)
	{
		//printf("get msg\n");
		//fseek(p_file, -2, SEEK_CUR);
		return 0;
	}
	while(1)
	{
		head[0] = head[1];
		if (fread(&head[1], 1, 1, p_file) != 1)
		{
			printf("find_msg_begain file end2\n");
			return -1;
		}
		
		if (head[0] == HEAD_BYTE1 && head[1] == HEAD_BYTE2)
		{
			//printf("get msg 2\n");
			//fseek(p_file, -2, SEEK_CUR);
			return 0;
		}
	}
    

}

uint8_t * write_var_ascii(FILE *p_file, char type, uint8_t *p)
{
	int len = 0;
	char buf[70];
	
	switch(type)
	{
		case 'f':
			fprintf(p_file, "%f", *((float*)p));
			len = 4;
			break;
		case 'e':	
		case 'i':
		case 'L':
			fprintf(p_file, "%d", *((int32_t*)p));
			len = 4;
			break;
			
		case 'E':
		case 'I':
			fprintf(p_file, "%u", *((uint32_t*)p));
			len = 4;
			break;
			
		case 'M':
		case 'B':
			fprintf(p_file, "%u", (unsigned int)*((uint8_t*)p));
			len = 1;
			break;
			
		case 'b':
			fprintf(p_file, "%d", (int)*((int8_t*)p));
			len = 1;
			break;
			
		case 'C':
		case 'H':
			fprintf(p_file, "%u", (unsigned int)*((uint16_t*)p));
			len = 2;
			break;
			
		case 'c':
		case 'h':
			fprintf(p_file, "%d", (int)*((int16_t*)p));
			len = 2;
			break;
			
		case 'Q':
			/*64bit CPU*/
			fprintf(p_file, "%lu", *((uint64_t*)p));
			len = 8;
			break;
			
		case 'q':
			/*64bit CPU*/
			fprintf(p_file, "%ld", *((uint64_t*)p));
			len = 8;
			break;
			
		case 'n':
			strncpy(buf, (char*)p, 4);
			buf[4] = '\0';
			fprintf(p_file, "%s", buf);
			len = 4;
			break;
			
		case 'N':
			strncpy(buf, (char*)p, 16);
			buf[16] = '\0';
			fprintf(p_file, "%s", buf);
			len = 16;
			break;
			
		case 'Z':
			strncpy(buf, (char*)p, 64);
			buf[64] = '\0';
			fprintf(p_file, "%s", buf);
			len = 64;
			break;
			
		default:
			printf("error! unsupport type: %c\n", type);
			return NULL;

		
	}
	
	return p+len;
}

int write_row_data_ascii(FILE *p_csv_file)
{
	int i = 0;
	int j = 0;
	unsigned char *p;
	
	for(i = 0; i < valid_msg_cnt; i++)
	{
		p = (unsigned char *)msg_table[i].p_data;
		j = 0;
		while(msg_table[i].body.format[j] != 0 && j < 16)
		{
			p = write_var_ascii(p_csv_file, msg_table[i].body.format[j], p);
			if (p == NULL)
			{
				printf("write_row_data_ascii error\n");
				return -1;
			}
			//printf("j=%d, struct_number=%d\n", j, msg_table[i].struct_number);
			if (i == (valid_msg_cnt - 1) && j == (msg_table[i].struct_number - 1))
			{
				fwrite("\r\n",1,2,p_csv_file);
				
			}
			else
			{
				fwrite(",",1,1,p_csv_file);
			}
			j++;
		}
		msg_table[i].data_not_updated = 0;
	}
	return 0;
}



int main(int argc, char *argv[])
{
	FILE *p_log_file = NULL; 
    FILE *p_csv_file = NULL; 
	char *filename_in = "in_data.csv";
	char filename_out[64] = {0};
	uint8_t msg_id = 0;
	int row_count = 0;
	int row_count2 = 0;
	int i = 0;
	
	if (argc >= 2)
	{
		filename_in = argv[1];
		strncpy(filename_out, filename_in, sizeof(filename_in));
		/*考虑改为倒叙搜索*/
		char *p = strchr(filename_out, '.');
		if(p == NULL)
		{
			p = filename_out + strlen(filename_out);
		}
		strncpy(p, ".csv\0", 5);
	}

	p_log_file = fopen(filename_in, "r");
	p_csv_file = fopen(filename_out, "w");

	if (p_log_file == NULL || p_csv_file == NULL)
	{
		printf("open file fail\n");
		return -1;
	}
	printf("convert %s to %s\n", filename_in, filename_out);

	if(parse_format_msg(p_log_file) < 0)
	{
		printf("find find_format_msg faild\n");
	}

	write_column_tile(p_csv_file);
	
	while (find_msg_begain(p_log_file) == 0)
	{
		if (fread(&msg_id, 1, 1, p_log_file) != 1)
		{
			printf("read msg_id EOF\n");
			break;
		}
		/*时间戳消息*/
		if (msg_id == 0x81)
		{
			row_count++;
			/*sdlog2_dump.py中存在，两行时间戳相等，换行并不是根据时间戳来的，时间戳有可能会写丢
			此时其他数据在本行有更新，要更改一下采用其他方式判断换行*/
			/*此处格式化成字符，写入CSV文件*/
			if (row_count > 1)
			{
				#if (NEW_LINE_MODE == 1)
				if (write_row_data_ascii(p_csv_file) < 0)
				{
					break;
				}
				#endif
			}
		}
		/*找到格式表中的ID，并把负载copy*/
		for(i = 0; i < valid_msg_cnt; i++)
		{
			if (msg_table[i].body.type == msg_id)
			{
				
				/*数据将要被覆盖了,判断一行新的方式*/
				if (1 == msg_table[i].data_not_updated)
				{
					row_count2++;
					//printf("msg_id=%d\n", msg_id);
					/*此处格式化成字符，写入CSV文件*/
					
					#if (NEW_LINE_MODE == 2)
					if (write_row_data_ascii(p_csv_file) < 0)
					{
						goto out;
					}
					#endif
				}

				if(fread(msg_table[i].p_data, 1, msg_table[i].body.length-LOG_PACKET_HEADER_LEN, p_log_file) != msg_table[i].body.length-LOG_PACKET_HEADER_LEN)
				{
					printf("read payload EOF\n");					
				}
				msg_table[i].data_not_updated = 1;
				break;
			}
		}
		if (msg_id == LOG_INFO_MSG)
		{
			if ((msg_table[i].body.length - LOG_PACKET_HEADER_LEN) == sizeof(struct log_INFO_s))
			{
				struct log_INFO_s *p_info = (struct log_INFO_s *)msg_table[i].p_data ;
				printf("uid:%s\n", p_info->fc_uid);
				printf("fc_ver:%s\n", p_info->fc_ver);
				printf("amu_ver:%s\n", p_info->amu_ver);
				printf("geo_ver:%s\n", p_info->geo_ver);
				printf("uav_type:%d\n", p_info->uav_type);
				printf("system_type:%d\n", p_info->system_type);
				printf("ability:0x%x\n", p_info->ability);
			}
			else
			{
				printf("info struct not match\n");
			}

		}
		else if (msg_id == LOG_VER_MSG)
		{
			if ((msg_table[i].body.length - LOG_PACKET_HEADER_LEN) == sizeof(struct log_VER_s))
			{
				struct log_VER_s *p_ver = (struct log_VER_s *)msg_table[i].p_data ;
				printf("arch:%s\n", p_ver->arch);
				printf("fw_git:%s\n", p_ver->fw_git);

			}
			else
			{
				printf("ver struct not match\n");
			}
		}
		
	}
out:	
	printf("row_count= %d, row_count2=%d\n", row_count, row_count2);
	fclose(p_log_file);
	fclose(p_csv_file);
	free_mem();
	return 0;
}

