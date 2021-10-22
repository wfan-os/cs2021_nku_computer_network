#pragma once
#include <Windows.h>
#include <time.h>

#define CLIENTNAME_MAX_LENGTH 48	//我们将用户名作为客户端名，最大长度限制
#define MSG_QUEUE_MAX_LENGTH  256	//消息队列的最大长度
#define CLIENT_QUEUE_MAX_NUM  24	//最多允许的用户数量
#define BACKLOG 10					//listen等待队列的最大长度

//结构体，用于存储client的信息，同时也是client_queue里面的元素
struct client {
	struct client* prev, * next;		//指向client队列的前后元素
	SOCKET socket;						//存储client的socket信息
	struct sockaddr_in address;			//存储远端client的地址信息
	char client_name[CLIENTNAME_MAX_LENGTH]; //存储客户名称（即用户名）
	HANDLE client_thread;				//管理一个客户端线程，用于接受用户消息，并放在消息队列里
};

//消息队列里的元素
struct msg_queue_elem {
	time_t timep;
	int content_length;
	char content[0];
};

//消息队列：
//服务器所维护的一个相当重要的数据结构，用于存放client线程接受大所有消息
//消息队列的元素都是指向特定内存区域（线程所创建的堆区）的指针
//消息队列使用信号量进行并发控制，防止冲突
//消息队列采用FIFO方法控制消息进出
struct chatmsg_queue {
	struct msg_queue_elem* slots[MSG_QUEUE_MAX_LENGTH];			//消息队列里的元素，每个都指向一个content

	volatile int head;			//指向第一个元素 - 由主线程修改
	volatile int tail;			//指向最后一个元素 - 由client线程修改
								//这里使用volatile关键字是为了提醒编译器这两个数据是极易变化的，不能做优化（例如放到寄存器里）

	HANDLE queue_empty_space;		//信号量：指示消息队列还有多少剩余空间
	HANDLE queue_present_message;	//信号量：指示消息队列中还有多少消息
	HANDLE mq_lock;					//信号量：作为互斥锁使用，确保每次只能有一个线程访问消息队列
};

//用户队列：
//服务器维护的一个循环双向链表，用于存放注册后的用户
struct client_queue {
	volatile int count;				//记录队列里有多少元素
	struct client* head, * tail;	//指向队列的头和尾
	HANDLE cq_lock;					//信号量：实现互斥访问，同一时间只能有一个人访问队列
};