#pragma once

#include <stdlib.h>
#include <time.h>
#include <winsock.h>

//ħ��
#define MAGIC_WXF 0x77286600
//���������
#define CONTENT_MAX_LENGTH 4096
//���������
#define USERNAME_MAX_LENGTH  48


struct msg_header
{
	int magic;
	int command;
	int command_code;
	time_t timep;
	unsigned int content_length;
};

struct exchg_msg
{
	struct msg_header head;
//ע�⣬�����ǿɱ䳤�ȵ�content�����Ǹ��䳤���飬��ռ�ռ䣬ֻ�Ǹ����Ű���
	char	   content[0];
};

/*********************************ָ��*************************************/
//�û�ָ��
#define	CMD_CLIENT_REGISTER 100		 //����ע���û�
#define CMD_CLIENT_DEPART	101		 //�����뿪������
#define CMD_CLIENT_SEND		102		 //�������������Ϣ

//������ָ��
#define CMD_SERVER_JOIN_OK	 200	 //�ɴӷ��������ص���Ϣ��Я������ʾ�������ѳɹ������ϸ��û�
#define CMD_SERVER_BROADCAST 201	 //�������ɹ�ע����û�
#define CMD_SERVER_CLOSE	 202	 //�������ر�
#define CMD_SERVER_FAIL		 203	 //���������ϣ��򱨴�

/*********************************������**********************************/
//��CMD_SERVER_FAIL�����Ĵ�����
#define ERR_JOIN_DUP_NAME       300 //�������������Ͻ������û���
#define ERR_UNKNOWN_CMD         301 //δָ֪��
#define ERR_OTHERS              302 //��������