#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

#include "chat.h"
#include "server.h"
#include <stdio.h>
#include <winsock.h>
#include <string.h>
#include <signal.h>

#define INFO	"[ INFO  ]"
#define ERR		"[ ERROR ]"
#define OK		"[ OK    ]"	
#define GET		"[ GET   ]"
#define JOIN	"[ JOIN  ]"
#define WARN	"[WARNING]"
#define SEND	"[ SEND  ]"
#define DEPART	"[ DEPART]"

struct client client_A, client_B;	//双人聊天，两个客户端
struct chatmsg_queue msg_queue;	//消息队列

SOCKET g_socket_srv;				//全局变量：服务器使用的socket
HANDLE broadcast_thread;			//广播线程
/*-------------------------------函数声明--------------------------------*/
void server_init(void);
void server_run(void);

//从客户端收听消息
DWORD WINAPI client_thread_fn(void* arg);
//向客户端广播消息
DWORD WINAPI broadcast_thread_fn(void* arg);
//向消息队列里加入消息，注意，add_msg里的指针是不能free的，只有被broadcast_thread_fn取出后才能free
void add_msg(struct msg_queue_elem *client_msg);
//向客户端发送消息，成功返回0，失败返回-1
int send_msg_to_client(SOCKET sock, char* msg, int command, int command_code, time_t timep);
void shutdown_handler(int signum);
void get_time(void);
void print_logo(void);

/*-------------------------------代码区---------------------------------*/
void server_init(void) {
	// TODO:
	// 1. 获取端口和自身IP
	// 2. WSAStartup()、初始化socket
	// 3. 初始化client和msg_queue（包括对socket、thread、状态以及信号量的初始化）

	int port = 0;

	//获取服务器端口
	do {
		char userIn[8];

		get_time();
		printf("%s Input the port for wait the connections from clients: ", GET);
		memset(userIn, 0, sizeof(userIn));
		gets(userIn);
		port = atoi(userIn);
		if (port <= 0 || port >= 65536) {
			get_time();
			printf("%s Port not valid! Retry\n", WARN);
		}
		else if (port <= 1024) {
			get_time();
			printf("%s Ports under 1024 aren't usable! Retry\n", WARN);
		}
	} while (port <= 1024 || port >= 65536);

	//启动WSA，获取本机IP地址并打印
	// WSAStartup
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("%s Failed WSAStartup() with error: %d\n", ERR, WSAGetLastError());
		perror("WSAStartup");
		return;
	}
	get_time();
	printf("%s WSAStartup Complete!\n", OK);

	//获取主机名，并通过主机名获取本机IP
	char name[256];
	PHOSTENT hostinfo;
	LPCSTR ip_addr = NULL;
	//获取主机名
	if (gethostname(name, sizeof(name)) == 0) {
		//根据主机名获取主机信息
		if ((hostinfo = gethostbyname(name)) != NULL) {
			//利用inet_ntoa将网络地址转化为字符串格式
			ip_addr = inet_ntoa(*(struct in_addr*)*hostinfo->h_addr_list);
		}
		else {
			printf("%s Get host info by name error.\n", ERR);
			perror("gethostbyname");
		}
	}
	else {
		printf("%s Get host name error.\n", ERR);
		perror("gethostname");
	}

	get_time();
	printf("%s Local Machine IP Adrees: %s\n", INFO, ip_addr);
	
	//创建socket
	if ((g_socket_srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR) {
		printf("%s Failed main socket creation!\n", ERR);
	}
	get_time();
	printf("%s Socket created!\n", OK);

	struct sockaddr_in address_srv;

	address_srv.sin_family = AF_INET;
	address_srv.sin_port = htons(port);
	address_srv.sin_addr.s_addr = inet_addr(ip_addr);  //采用默认值，令socket绑定绑定本机IP

	//server socket绑定
	if (bind(g_socket_srv, (struct sockaddr*)&address_srv, sizeof(struct sockaddr)) == SOCKET_ERROR) {
		get_time();
		printf("%s Failed main socket binding! %d\n", ERR, WSAGetLastError());
	}
	get_time();
	printf("%s Bind Success!\n", OK);

	//初始化client
	client_A.occupied = client_B.occupied = 0;
	client_A.socket = client_B.socket = INVALID_SOCKET;
	client_A.client_thread = client_B.client_thread = NULL;
	memset(&client_A.address, 0, sizeof(struct sockaddr_in));
	memset(&client_B.address, 0, sizeof(struct sockaddr_in));
	memset(client_A.client_name, 0, CLIENTNAME_MAX_LENGTH);
	memset(client_B.client_name, 0, CLIENTNAME_MAX_LENGTH);

	//初始化chatmsg_queue
	msg_queue.head = msg_queue.tail = 0;
	for (int i = 0;i < MSG_QUEUE_MAX_LENGTH;i++) {
		msg_queue.slots[i] = NULL;
	}
	if ((msg_queue.queue_empty_space = CreateSemaphore(NULL, MSG_QUEUE_MAX_LENGTH, MSG_QUEUE_MAX_LENGTH, NULL)) == NULL) {
		get_time();
		printf("%s Failed in initializing semaphore!\n", ERR);
		perror("Semaphore Create\n");
	}
	if ((msg_queue.queue_present_message = CreateSemaphore(NULL, 0, MSG_QUEUE_MAX_LENGTH, NULL)) == NULL) {
		get_time();
		printf("%s Failed in initializing semaphore!\n", ERR);
		perror("Semaphore Create\n");
	}
	if ((msg_queue.mq_lock = CreateSemaphore(NULL, 1, 1, NULL)) == NULL) {
		get_time();
		printf("%s Failed in initializing semaphore!\n", ERR);
		perror("Semaphore Create\n");
	}

	//创建广播线程
	if ((broadcast_thread = CreateThread(NULL, 0, broadcast_thread_fn, NULL, 0, NULL)) == NULL) {
		printf("%s Can't create broadcast thread!\n", ERR);
		perror("Broadcast thread create fail\n");
	}
	else {
		get_time();
		printf("%s Broadcast thread create success!\n", INFO);
	}

	return;
}
void server_run(void) {
	while (1) {
		// TODO:
			// 1. 监听新的连接请求，听到后就accept
			// 2. 如果传来的是CMD_CLIENT_REGIST，我们会将client_A或client_B分配给它，并为其创建一个线程，并返回SERVER_JOIN_OK
			//    2.1 要求一：client_A、client_B不是全部处于占用状态
			//    2.2 要求二：不与其它客户端重名
			// 3. 其他情况将传回

		struct sockaddr_in new_addr;
		struct msg_header new_client_msg_head;
		int recv_cmd = 0;
		int recv_magic = 0;
		int recv_content_length = 0;
		SOCKET new_conn_sock = INVALID_SOCKET;

		get_time();
		printf("%s Start listening...\n", INFO);
		if (listen(g_socket_srv, BACKLOG) == SOCKET_ERROR) {
			printf("%s Socket listen error!\n", ERR);
		}

		get_time();
		printf("%s Starting accept...\n", INFO);

		int tmp = sizeof(struct sockaddr_in);
		if ((new_conn_sock = accept(g_socket_srv, (struct sockaddr*)&new_addr, (int*) &tmp)) == INVALID_SOCKET) {
			printf("%s Socket accept error: %d\n", ERR, WSAGetLastError());
		}

		get_time();
		printf("%s Accept Success!\n", OK);

		if (recv(new_conn_sock, (char*)&new_client_msg_head, sizeof(struct msg_header), 0) == SOCKET_ERROR) {
			printf("%s Socket receive error!\n", ERR);
			perror("receive error\n");
		}
		recv_magic = ntohl(new_client_msg_head.magic);
		if (recv_magic != MAGIC_WXF) {
			printf("%s Unknown data!\n", ERR);
		}
		recv_cmd = ntohl(new_client_msg_head.command);
		printf("%s receive success--server_run!\n", OK);

		//用户试图注册
		if (recv_cmd == CMD_CLIENT_REGIST) {
			//获取注册的用户名
			char* regist_name = NULL;
			if (new_client_msg_head.content_length <= 0) {
				get_time();
				printf("%s Client register with a null name!\n", ERR);
				perror("client register error\n");
			}
			recv_content_length = ntohl(new_client_msg_head.content_length);
			regist_name = (char*)malloc(recv_content_length);
			if (recv(new_conn_sock, regist_name, recv_content_length, 0) == SOCKET_ERROR) {
				printf("%s Socket receive error!\n", ERR);
				perror("receive error\n");
			}
			if (recv_magic != MAGIC_WXF) {
				printf("%s Unknown data!\n", ERR);
			}

			//先查看是否还有空位，如果还有空位，那么继续
			if (!client_A.occupied || !client_B.occupied) {
				//出现重名现象，发送重名错误
				if (!strcmp(regist_name, client_A.client_name) || !strcmp(regist_name, client_B.client_name)) {
					if (send_msg_to_client(new_conn_sock, NULL, CMD_SERVER_FAIL, ERR_JOIN_DUP_NAME, time(NULL)) != 0) {
						printf("%s Socket send error!\n", ERR);
					}
				}

				struct client* client_p = NULL;

				//如果A未被占用
				if (!client_A.occupied) {
					client_p = &client_A;
				}
				//A已占用，但如果B未被占用
				else if (!client_B.occupied) {
					client_p = &client_B;
				}

				//赋值
				if (client_p) {
					client_p->address = new_addr;
					strcpy(client_p->client_name, regist_name);
					client_p->occupied = 1;
					client_p->socket = new_conn_sock;

					client_p->client_thread = NULL;
					if ((client_p->client_thread = CreateThread(NULL, 0, client_thread_fn, client_p, 0, NULL)) == NULL) {
						printf("%s Can't creat client thread\n", ERR);
						perror("thread create error\n");
					}
					else {
						get_time();
						printf("%s Client thread create success!\n", OK);
					}
				}
				//发生了很奇怪的错误，因为两个至少得有一个是空闲的（参照之前的逻辑）
				else {
					printf("%s Unknow Error in giving new registed one a client object\n", ERR);
				}

				free(regist_name);
			}
			else {
				//此处说明人数已满，要求过会儿再加入
				if (send_msg_to_client(new_conn_sock, NULL, CMD_SERVER_FAIL, ERR_JOIN_ROOM_FULL, time(NULL)) != 0) {
					printf("%s Socket send error!\n", ERR);
				}
			}
		}
		else {
			if (send_msg_to_client(new_conn_sock, NULL, CMD_SERVER_FAIL, ERR_UNKNOWN_CMD, time(NULL)) != 0) {
				printf("%s Socket send error!\n", ERR);
				perror("send error\n");
			}
		}
	}

	WSACleanup();
	return 0;
}

DWORD WINAPI broadcast_thread_fn(void* arg) {
	// TODO:
	// 将 msg_queue 里面的事情提取出来并发送给所有客户端

	while (1) {
		if (WaitForSingleObject(msg_queue.queue_present_message, INFINITE) == WAIT_ABANDONED) {
			printf("%s queue_present_message semaphore wait fail!\n", ERR);
			perror("semaphore wait fail\n");
		}
		if (WaitForSingleObject(msg_queue.mq_lock, INFINITE) == WAIT_ABANDONED) {
			printf("%s mq_lock semaphore wait fail!\n", ERR);
			perror("semaphore wait fail\n");
		}

		int msg_head = msg_queue.head;
		struct msg_queue_elem *mbuf = msg_queue.slots[msg_head];

		if (client_A.occupied) {
			if (send_msg_to_client(client_A.socket, mbuf->content, CMD_SERVER_BROADCAST, 0, mbuf->timep) != 0) {
				printf("%s broadcast send error!\n", ERR);
			}
		}
		if (client_B.occupied) {
			if (send_msg_to_client(client_B.socket, mbuf->content, CMD_SERVER_BROADCAST, 0, mbuf->timep) != 0) {
				printf("%s broadcast send error!\n", ERR);
			}
		}

		free(msg_queue.slots[msg_head]);

		msg_queue.head = (msg_queue.head + 1) % MSG_QUEUE_MAX_LENGTH;

		if (ReleaseSemaphore(msg_queue.mq_lock, 1, NULL) == NULL) {
			printf("%s mq_lock semaphore release fail!\n", ERR);
			perror("semaphore release fail\n");
		}
		if (ReleaseSemaphore(msg_queue.queue_empty_space, 1, NULL) == NULL) {
			printf("%s queue_present_message semaphore release fail!\n", ERR);
			perror("semaphore release fail\n");
		}
	}

	return 0;
}
DWORD WINAPI client_thread_fn(void* arg) {
	// TODO：
	// 1. 欢迎用户加入
	// 2. 收听用户消息
	//    2.1 如果是CMD_CLIENT_SEND，把消息放到msg_queue里
	//    2.2 如果是CMD_CLIENT_DEPART
	//		  2.2.1 向其它用户通知该用户已离开
	//        2.2.2 销毁该用户占用的所有资源，重置对应的client结构
	//        2.2.3 终止本线程
	//	  2.3 如果用户掉线，则同CMD_CLIENT_DEPART

	struct client* ptr_client = (struct client*)arg;
	struct msg_header new_client_msg_header;
	struct msg_queue_elem* message = NULL;
	int about_to_depart = 0;	//这个变量表示该用户是否离开

	char tmp[128];
	strcpy(tmp, ptr_client->client_name);
	strcat(tmp, " just joins, welcome!");
	get_time();
	printf("%s %s\n", JOIN, tmp);
	message = (struct msg_queue_elem*)malloc(sizeof(struct msg_queue_elem) + strlen(tmp) + 1);
	message->content_length = strlen(tmp) + 1;
	message->timep = time(NULL);
	strcpy(message->content, tmp);

	add_msg(message);

	if (send_msg_to_client(ptr_client->socket, NULL, CMD_SERVER_JOIN_OK, 0, time(NULL)) == -1)
	{
		get_time();
		printf("%s JOIN_OK send error!\n", ERR);
		perror("send error\n");
	}

	while (1) {
		memset(&new_client_msg_header, 0, sizeof(struct msg_header));
		int recv_cmd = 0;

		//如果receive收听到的消息是SOCKET_ERROR，即收不到心跳包，说明用户掉线了，我们按DEPART处理
		if (recv(ptr_client->socket, (char*)&new_client_msg_header, sizeof(struct msg_header), 0) == SOCKET_ERROR) {
			get_time();
			printf("%s Client %s has cancled connection.\n", INFO, ptr_client->client_name);
			about_to_depart = 1;
			break;
		}
		if (ntohl(new_client_msg_header.magic) != MAGIC_WXF) {
			printf("%s Unknown data!\n", ERR);
		}
		
		recv_cmd = ntohl(new_client_msg_header.command);
		
		if (recv_cmd == CMD_CLIENT_SEND) {
			char* tmp1 = NULL;
			char* tmp2 = NULL;
			struct msg_queue_elem* tmp_message = NULL;
			int msg_len = ntohl(new_client_msg_header.content_length);

			if (msg_len <= 1) {
				printf("%s Received a null message!\n", ERR);
			}

			tmp1 = (char*)malloc(msg_len);
			//继续recv信息，收集后续的内容
			if (recv(ptr_client->socket, tmp1, msg_len, 0) == SOCKET_ERROR) {
				printf("%s Client %s has receive error: %d\n", ERR, ptr_client->client_name, WSAGetLastError());
				perror("recv error\n");
			}

			tmp2 = (char*)malloc(strlen(ptr_client->client_name) + 2 + msg_len);
			strcpy(tmp2, ptr_client->client_name);
			strcat(tmp2, ": ");
			strcat(tmp2, tmp1);

			get_time();
			printf("%s %s\n", SEND, tmp2);

			tmp_message = (struct msg_queue_elem*)malloc(sizeof(struct msg_queue_elem) + strlen(tmp2));
			tmp_message->content_length = strlen(tmp2);
			tmp_message->timep = ntohl(new_client_msg_header.timep);
			strcpy(tmp_message->content, tmp2);
			add_msg(tmp_message);
			free(tmp1);
			free(tmp2);
		}
		else if (recv_cmd == CMD_CLIENT_DEPART) {
			about_to_depart = 1;
			break;
		}
		else if (recv_cmd == NULL) {
			get_time();
			printf("%s recv ERROR and no cmd here\n", ERR);
		}
		else {
			get_time();
			printf("%s Unknown command -- client thread\n", ERR);
		}
	}

	if (about_to_depart) {
		printf("%s Client Depart!\n", DEPART);

		char *c = NULL;
		c = (char*) malloc(strlen(ptr_client->client_name) + strlen(" just leaves the chat room, goodbye!\n"));
		strcpy(c, ptr_client->client_name);
		strcat(c, " just leaves the chat room, goodbye!\n");

		struct msg_queue_elem* msg_elem = NULL;
		msg_elem = (struct msg_queue_elem*)malloc(sizeof(struct msg_queue_elem) + strlen(c));
		
		msg_elem->content_length = strlen(c);
		msg_elem->timep = time(NULL);
		strcpy(msg_elem->content, c);

		get_time();
		printf("%s %s\n", DEPART, c);
		add_msg(msg_elem);
		free(c);
	}

	//重置对应的client结构
	memset(ptr_client, 0, sizeof(struct client));
	return 0;
}
void add_msg(struct msg_queue_elem* client_msg) {
	//我们要消耗掉msg_queue的一个空余空间
	if (WaitForSingleObject(msg_queue.queue_empty_space, INFINITE) == WAIT_ABANDONED) {
		printf("%s queue_empty_space semaphore wait fail!\n", ERR);
		perror("semaphore wait error\n");
	}

	//利用信号量mq_lock实现msg_queue访问互斥
	if (WaitForSingleObject(msg_queue.mq_lock, INFINITE) == WAIT_ABANDONED) {
		printf("%s mq_lock semaphore wait fail!\n", ERR);
		perror("semaphore wait error\n");
	}

	int msgTail = msg_queue.tail;
	msg_queue.slots[msgTail] = client_msg;
	msg_queue.tail = (msg_queue.tail + 1) % MSG_QUEUE_MAX_LENGTH;

	if (ReleaseSemaphore(msg_queue.mq_lock, 1, NULL) == NULL) {
		printf("%s mq_lock semaphore release fail!\n", ERR);
		perror("semaphore release fail\n");
	}

	if (ReleaseSemaphore(msg_queue.queue_present_message, 1, NULL) == NULL) {
		printf("%s queue_present_message release fail!\n", ERR);
		perror("semaphore release fail\n");
	}
}
int send_msg_to_client(SOCKET sock, char* msg, int command, int command_code, time_t timep) {
	struct exchg_msg *mbuf = NULL;
	int msg_len = 0;

	if (msg) {
		msg_len = strlen(msg);
		msg_len++;
	}
	mbuf = (struct exchg_msg*)malloc(sizeof(struct exchg_msg) + msg_len);
	memset(mbuf, 0, sizeof(struct exchg_msg) + msg_len);

	mbuf->head.magic = htonl(MAGIC_WXF);
	mbuf->head.command = htonl(command);
	mbuf->head.command_code = htonl(command_code);
	mbuf->head.timep = htonl(timep);
	mbuf->head.content_length = htonl(msg_len);

	if(command==CMD_SERVER_BROADCAST) strcpy(mbuf->content, msg);

	if (send(sock, (char*)mbuf, sizeof(struct exchg_msg) + msg_len, 0) == INVALID_SOCKET) {
		get_time();
		printf("%s Client socket send error!\n", ERR);
		perror("send error\n");
		return -1;
	}

	free(mbuf);
	return 0;
}
void shutdown_handler(int signum) {
	// TODO:
	// 1, 向所有用户发送CMD_SERVER_CLOSE
	// 2. 终结两个客户线程和广播线程
	// 3. 销毁所有分配的资源：内存，信号量，等

	//向客户端发送消息
	if (client_A.occupied) {
		if (send_msg_to_client(client_A.socket, NULL, CMD_SERVER_CLOSE, 0, time(NULL)) != 0) {
			get_time();
			printf("%s shutdown send error\n", ERR);
		}
	}
	if (client_B.occupied) {
		if (send_msg_to_client(client_B.socket, NULL, CMD_SERVER_CLOSE, 0, time(NULL)) != 0) {
			get_time();
			printf("%s shutdown send error\n", ERR);
		}
	}

	DWORD dw_broadcast_exit_code = 0;
	GetExitCodeThread(broadcast_thread, &dw_broadcast_exit_code);
	TerminateThread(broadcast_thread, dw_broadcast_exit_code);
	WaitForSingleObject(broadcast_thread, INFINITE);

	//终结client线程并销毁socket
	if (client_A.occupied) {
		DWORD dw_client_exit_code = 0;
		GetExitCodeThread(client_A.client_thread, &dw_client_exit_code);
		TerminateThread(client_A.client_thread, dw_client_exit_code);
		WaitForSingleObject(client_A.client_thread, INFINITE);
		CloseHandle(client_A.client_thread);
		CloseHandle(client_A.socket);
	}
	if (client_B.occupied) {
		DWORD dw_client_exit_code = 0;
		GetExitCodeThread(client_B.client_thread, &dw_client_exit_code);
		TerminateThread(client_B.client_thread, dw_client_exit_code);
		WaitForSingleObject(client_B.client_thread, INFINITE);
		CloseHandle(client_B.client_thread);
		CloseHandle(client_B.socket);
	}

	client_A.occupied = 0;
	client_B.occupied = 0;

	//销毁资源
	//销毁信号量
	if (CloseHandle(msg_queue.mq_lock) == NULL) {
		get_time();
		printf("%s semaphore detroy fail!\n", ERR);
	}
	if (CloseHandle(msg_queue.queue_empty_space) == NULL) {
		get_time();
		printf("%s semaphore detroy fail!\n", ERR);
	}
	if (CloseHandle(msg_queue.queue_present_message) == NULL) {
		get_time();
		printf("%s semaphore detroy fail!\n", ERR);
	}

	//释放msg_queue的内存资源
	for (int i = 0;i < MSG_QUEUE_MAX_LENGTH;i++) {
		if (msg_queue.slots[i] != NULL) {
			free(msg_queue.slots[i]);
		}
	}
	
	//销毁服务器的socket
	CloseHandle(g_socket_srv);

	exit(0);
}

int main(int argc, char* argv[]) {
	print_logo();

	get_time();
	printf("%s Start Server Manager\n", INFO);

	//注册"Ctrl+C"为信号并赋予其处理方式，这样，当我们按下Ctrl+C时，程序就会跳转到shutdown_handler，实现更好的退出
	signal(SIGINT, shutdown_handler);
	signal(SIGTERM, shutdown_handler);

	//初始化服务器
	server_init();

	//运行服务器
	server_run();

	return 0;
}

void get_time(void) {
	char s[100];
	time_t t = time(NULL);
	struct tm* tp = localtime(&t);

	strftime(s, 100, "%H:%M:%S", tp);
	printf("%s ", s);
}
void print_logo(void) {
	printf("__________________________________________________________________________________________________________________________\n\n");
	printf("  /$$$$$$  /$$                   /$$            /$$$$$$                                                   \n");
	printf(" /$$__  $$| $$                  | $$           /$$__  $$                                                  \n");
	printf("| $$  \\__/| $$$$$$$   /$$$$$$  /$$$$$$        | $$  \\__/  /$$$$$$   /$$$$$$  /$$    /$$ /$$$$$$   /$$$$$$ \n");
	printf("| $$      | $$__  $$ |____  $$|_  $$_/        |  $$$$$$  /$$__  $$ /$$__  $$|  $$  /$$//$$__  $$ /$$__  $$\n");
	printf("| $$      | $$  \\ $$  /$$$$$$$  | $$           \\____  $$| $$$$$$$$| $$  \\__/ \\  $$/$$/| $$$$$$$$| $$  \\__/\n");
	printf("| $$    $$| $$  | $$ /$$__  $$  | $$ /$$       /$$  \\ $$| $$_____/| $$        \\  $$$/ | $$_____/| $$      \n");
	printf("|  $$$$$$/| $$  | $$|  $$$$$$$  |  $$$$/      |  $$$$$$/|  $$$$$$$| $$         \\  $/  |  $$$$$$$| $$      \n");
	printf(" \\______/ |__/  |__/ \\_______/   \\___/         \\______/  \\_______/|__/          \\_/    \\_______/|__/      By Wang Xiaofan\n\n");
	printf("__________________________________________________________________________________________________________________________\n\n");
	printf("\n");
	printf("\n");
}