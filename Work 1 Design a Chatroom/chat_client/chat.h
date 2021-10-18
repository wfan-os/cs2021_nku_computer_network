#pragma once

#include <stdlib.h>
#include <time.h>
#include <winsock.h>

//魔数
#define MAGIC_WXF 0x77286600
//正文最长限制
#define CONTENT_MAX_LENGTH 4096
//名字最长限制
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
//注意，我们是可变长度的content，这是个变长数组，不占空间，只是个符号罢了
	char	   content[0];
};

/*********************************指令*************************************/
//用户指令
#define	CMD_CLIENT_REGISTER 100		 //请求注册用户
#define CMD_CLIENT_DEPART	101		 //请求离开服务器
#define CMD_CLIENT_SEND		102		 //向服务器发送消息

//服务器指令
#define CMD_SERVER_JOIN_OK	 200	 //由从服务器返回的消息中携带，表示服务器已成功连接上该用户
#define CMD_SERVER_BROADCAST 201	 //服务器成功注册该用户
#define CMD_SERVER_CLOSE	 202	 //服务器关闭
#define CMD_SERVER_FAIL		 203	 //服务器故障（或报错）

/*********************************错误码**********************************/
//由CMD_SERVER_FAIL产生的错误码
#define ERR_JOIN_DUP_NAME       300 //重名（服务器严禁重名用户）
#define ERR_UNKNOWN_CMD         301 //未知指令
#define ERR_OTHERS              302 //其它错误