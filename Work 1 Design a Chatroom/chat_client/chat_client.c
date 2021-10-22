#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

#include "chat.h"
#include <stdio.h>
#include <winsock.h>
#include <string.h>

#define INFO	"[ INFO  ]"
#define ERR		"[ ERROR ]"
#define OK		"[ OK    ]"	



//全局变量，由主线程和子线程使用，表示客户端和服务器连接的socket
SOCKET g_sock;
//全局变量，接受线程（用于收听服务器消息），其中，与服务器建立连接并向其发送消息由主线程完成
HANDLE recv_thread;

int send_msg_to_server(SOCKET sock, char* msg, int command, int command_code);
DWORD WINAPI recv_thread_fn(void* arg);
void print_logo(void);
void get_time(void);

//DEBUG用函数
void dbg_print_recv(char* msg);


int main()
{
	char input_buffer[CONTENT_MAX_LENGTH * 2];		//获取用户输入内容
	char* user_command, * user_command_parameter;	//（临时）用户输入中提取的header内容
	char username[USERNAME_MAX_LENGTH];				//存储用户名
	struct sockaddr_in server_addr;					//存储服务器地址
	unsigned long server_ip = 0;					//（临时）存放ip
	int server_port = 0;							//（临时）存放端口
	int is_connected = 0;							//连接状态（0表示断开）

	//初始化变量
	user_command = NULL;
	user_command_parameter = NULL;
	memset(username, 0, sizeof(username));
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	//循环，直到用户DEPART
	while (1) {
		if (!is_connected) {

			//获取IP和端口
			do {
				printf("Input the IP of server:");
				memset(input_buffer, 0, sizeof(input_buffer));
				gets(input_buffer);
				server_ip = inet_addr(input_buffer);
				do {
					printf("Input the Port of server: ");
					memset(input_buffer, 0, sizeof(input_buffer));
					gets(input_buffer);
					server_port = atoi(input_buffer);
					if (server_port <= 0 || server_port >= 65536)
						printf("Port not valid! Retry\n");
				} while (server_port <= 0 || server_port >= 65536);
				printf("Server IP: %s\nServer Port: %d\nContinue? [Y/n] ", inet_ntoa(*(struct in_addr*)&server_ip), server_port);
				memset(input_buffer, 0x00, sizeof(input_buffer));
				gets(input_buffer);
				if (input_buffer[0] == '\n' || input_buffer[0] == '\r')
					input_buffer[0] = 'Y';
			} while (input_buffer[1] == '\0' && input_buffer[0] != 'Y' && input_buffer[0] != 'y');
			
			// WSAStartup
			WSADATA wsaData;
			if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
				printf("%s Failed WSAStartup() with error: %d\n", ERR, WSAGetLastError());
				perror("WSAStartup");
				return 1;
			}
			get_time();
			printf("%s WSAStartup Complete!\n", OK);

			//创建socket
			g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (g_sock == INVALID_SOCKET) {
				get_time();
				printf("%s Failed main socket creation: %d\n", ERR, WSAGetLastError());
				return 2;
			}
			get_time();
			printf("%s Socket created!\n", OK);

			//服务器地址填入
			server_addr.sin_family = AF_INET;
			server_addr.sin_port = htons(server_port);
			server_addr.sin_addr.s_addr = server_ip;

			//连接服务器
			if (connect(g_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
				is_connected = 0;
				get_time();
				printf("%s Connection failed: %d\n", ERR, WSAGetLastError());
				perror("connect failure");
				if (g_sock != INVALID_SOCKET) {
					closesocket(g_sock);
					g_sock = INVALID_SOCKET;
				}
				return 0;
			}
			else {
				get_time();
				printf("%s Server connected successfully!\n", INFO);
			}

			//注册用户，如果输入用户名不合格，抑或是服务器那边不通过就会一直循环
			while (1) {
				//获取用户名
				do {
					memset(username, 0, sizeof(username));
					printf("Enter Username [MAX 48 CHAR]: ");
					gets(username);
					printf("\n");
					if (strlen(username) <= 0 || strlen(username) >= 48)
						printf("Username not valid! Retry\n");
				} while (strlen(username) <= 0 || strlen(username) >= 48);

				//向服务器发送消息，要求注册用户
				if (send_msg_to_server(g_sock, username, CMD_CLIENT_REGISTER, 0) == -1) {
					get_time();
					printf("%s Send Message Failed!\n", ERR);
				}

				//从服务器接收消息，如果没能成功注册，就继续循环
				struct exchg_msg* mbuf;
				mbuf = (struct exchg_msg*)malloc(sizeof(struct exchg_msg) + CONTENT_MAX_LENGTH);
				memset(mbuf, 0, sizeof(struct exchg_msg) + CONTENT_MAX_LENGTH);
				if (recv(g_sock, (char*)mbuf, sizeof(struct exchg_msg) + CONTENT_MAX_LENGTH, 0) == SOCKET_ERROR) {
					get_time();
					printf("%s Receive Message Error: %d\n", ERR, WSAGetLastError());
					perror("receive\n");
				}

				//dbg_print_recv((char*)mbuf);

				//服务器发过来OK，就代表成功了，退出循环
				//如果不OK，代表你跟另一个用户重名
				int reply_command = ntohl(mbuf->head.command);
				if (reply_command == CMD_SERVER_JOIN_OK) {
					break;
				}
				else if (reply_command == CMD_SERVER_FAIL) {
					int reply_command_code = ntohl(mbuf->head.command_code);
					if (reply_command_code == ERR_JOIN_DUP_NAME) {
						get_time();
						printf("%s Connection failure - your name has been used, pls change your name.\n", ERR);
					}
					else if (reply_command_code == ERR_JOIN_ROOM_FULL) {
						get_time();
						printf("%s Connection failure - the room is full now, pls register later.\n", ERR);
					}
					else {
						get_time();
						printf("%s connection failure - unknown error\n", ERR);
						perror("recv from srv - an unknow error\n");
					}
				}
			}

			//创建接受消息的线程
			if ((recv_thread = CreateThread(NULL, 0, recv_thread_fn, NULL, 0, NULL)) == NULL) {
				get_time();
				printf("%s Fail to start the background thread!\n", ERR);
				perror("Thread Create");
				if (g_sock != INVALID_SOCKET)
				{
					closesocket(g_sock);
					g_sock = INVALID_SOCKET;
				}
				return 0;
			}

			//至此，我们已经注册好了用户并形成了稳定的连接
			is_connected = 1;
			//现在，我们清屏开始正式聊天
			system("cls");
			print_logo();
			printf("\n> ");
		}

		memset(input_buffer, 0, sizeof(input_buffer));
		gets(input_buffer);

		//获取用户的命令
		user_command = strtok(input_buffer, " ");

		if (!user_command) continue;

		//获取命令参数
		user_command_parameter = strtok(NULL, "\n");

		if (_stricmp(user_command, "CLEAR") == 0) {
			system("cls");
			print_logo();
		}

		else if (_stricmp(user_command, "SEND") == 0) {
			if (!is_connected) {
				get_time();
				printf("%s You cannot send any message if it is not connected!\n", ERR);
				continue;
			}
			if (user_command_parameter == NULL) {
				get_time();
				printf("%s You cannot send an empty message\n", ERR);
				continue;
			}
			else if (send_msg_to_server(g_sock, user_command_parameter, CMD_CLIENT_SEND, 0) != 0) {
				get_time();
				printf("%s Send Message Failed!\n", ERR);
			}
		}

		else if (_stricmp(user_command, "DEPART") == 0) {
			if (is_connected) {
				//告诉服务器用户离开了
				if (send_msg_to_server(g_sock, NULL, CMD_CLIENT_DEPART, 0) != 0) {
					get_time();
					printf("%s Depart fails!\n", ERR);
				}

				//关闭socket
				if (g_sock != INVALID_SOCKET)
				{
					closesocket(g_sock);
					g_sock = INVALID_SOCKET;
				}
				is_connected = 0;

				get_time();
				printf("%s You have left the chat room.\n", INFO);
				get_time();
				printf("%s Disconnected.\n", INFO);

				system("pause");
				return 0;
			}
			else {
				get_time();
				printf("%s You can only depart with an existing connection\n", INFO);
			}
		}

		else {
			get_time();
			printf("%s Undifined command. Please use the told commands.\n", ERR);
		}
	}

	WSACleanup();
	return 0;
}

//向服务器发送消息，成功返回0，失败返回-1
int send_msg_to_server(SOCKET sock, char* msg, int command, int command_code) {
	struct exchg_msg* mbuf = NULL;
	time_t t = time(NULL);
	int msg_len = 0;

	if (msg) {
		msg_len = strlen(msg);
		msg_len++;
	}

	if (msg_len < 0) {
		perror("invalid message to send\n");
		return -1;
	}

	mbuf = (struct exchg_msg*)malloc(sizeof(struct exchg_msg) + msg_len);
	memset(mbuf, 0, sizeof(struct exchg_msg) + msg_len);

	mbuf->head.magic = htonl(MAGIC_WXF);
	mbuf->head.command = htonl(command);
	mbuf->head.command_code = htonl(command_code);
	mbuf->head.timep = htonl(t);

	if (command == CMD_CLIENT_DEPART) {
		//DEPART命令不需要任何内容
		mbuf->head.content_length = htonl(0);
	}
	else if (command == CMD_CLIENT_REGISTER || command == CMD_CLIENT_SEND) {
		//REGISTER时内容存储 (用户名)
		//SEND时内容存储 (用户名：Message)
		strcpy(mbuf->content, msg);
		mbuf->head.content_length = htonl(msg_len + 1);
	}

	if (send(sock, (char*) mbuf, sizeof(struct exchg_msg) + msg_len + 1, 0) == SOCKET_ERROR) {
		perror("send\n");
		printf("send %s with error: %d\n", (char*)mbuf, WSAGetLastError());
		return -1;
	}

	return 0;
}

//子线程，用于接受server发来的消息并直接打印在屏幕上，正常退出返回0，异常返回-1
DWORD WINAPI recv_thread_fn(void* arg) {

	struct exchg_msg* mbuf;
	mbuf = (struct exchg_msg*)malloc(sizeof(struct exchg_msg) + CONTENT_MAX_LENGTH);
	int reply_command = 0;
	int content_len = 0;
	int reply_command_code = 0;

	//一直监听消息
	while (1) {
		memset(mbuf, 0, sizeof(struct exchg_msg) + CONTENT_MAX_LENGTH);

		if (recv(g_sock, (char*)mbuf, sizeof(struct exchg_msg) + CONTENT_MAX_LENGTH, 0) == SOCKET_ERROR) {
			get_time();
			printf("%s You have disconnected with the server: %d\n", INFO);
			return 0;
		}

		reply_command = ntohl(mbuf->head.command);
		reply_command_code = ntohl(mbuf->head.command_code);
		content_len = ntohl(mbuf->head.content_length);

		if (reply_command == CMD_SERVER_BROADCAST) {
			char s[100];
			time_t t = ntohl(mbuf->head.timep);
			struct tm* tp = localtime(&t);
			strftime(s, 100, "%H:%M:%S", tp);
			printf("\n%s %s\n", s, mbuf->content);
			printf("\n> ");
		}
		else if (reply_command == CMD_SERVER_CLOSE) {
			get_time();
			printf("%s Please Exit: the chat server has closed.\n", INFO);
			return 0;
		}
		else if (reply_command == CMD_SERVER_FAIL) {
			if (reply_command_code == ERR_UNKNOWN_CMD) {
				get_time();
				printf("%s The Server says that the command you sent is unknown.\n", ERR);
			}
			else if (reply_command_code == ERR_OTHERS) {
				get_time();
				printf("%s The Server returns an error.\n", ERR);
			}
			else {
				get_time();
				printf("%s The Server says that there is an error but we don't know what it is.\n", ERR);
			}
			return -1;
		}
		else {
			get_time();
			printf("%s Receive Thread has got a wrong message.\n", ERR);
			return -1;
		}
	}
	return 0;
}


void print_logo(void) {
	printf("_______________________________________________________________________________________________________________\n\n");
	printf("   /$$$$$$  /$$                   /$$           /$$$$$$$                                   \n");
	printf("  /$$__  $$| $$                  | $$          | $$__  $$                                  \n");
	printf(" | $$  \\__/| $$$$$$$   /$$$$$$  /$$$$$$        | $$  \\ $$  /$$$$$$   /$$$$$$  /$$$$$$/$$$$ \n");
	printf(" | $$      | $$__  $$ |____  $$|_  $$_/        | $$$$$$$/ /$$__  $$ /$$__  $$| $$_  $$_  $$\n");
	printf(" | $$      | $$  \\ $$  /$$$$$$$  | $$          | $$__  $$| $$  \\ $$| $$  \\ $$| $$ \\ $$ \\ $$\n");
	printf(" | $$    $$| $$  | $$ /$$__  $$  | $$ /$$      | $$  \\ $$| $$  | $$| $$  | $$| $$ | $$ | $$\n");
	printf(" |  $$$$$$/| $$  | $$|  $$$$$$$  |  $$$$/      | $$  | $$|  $$$$$$/|  $$$$$$/| $$ | $$ | $$\n");
	printf("  \\______/ |__/  |__/ \\_______/   \\___/        |__/  |__/ \\______/  \\______/ |__/ |__/ |__/ By Wang Xiaofan\n\n");
	printf("_______________________________________________________________________________________________________________\n\n");
	printf("                                   [CLEAR] [SEND] [DEPART]\n");
	printf("\n");
	printf("\n");
}
void get_time(void) {
	char s[100];
	time_t t = time(NULL);
	struct tm* tp = localtime(&t);

	strftime(s, 100, "%H:%M:%S", tp);
	printf("%s ", s);
}


void dbg_print_recv(char* msg) {
	printf("--------------------------------------------DEBUG----------------------------------------------------\n");
	printf("%s\n", msg);
	printf("--------------------------------------------DEBUG----------------------------------------------------\n");
}