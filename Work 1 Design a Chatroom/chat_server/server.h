#pragma once
#include <Windows.h>
#include <time.h>

#define CLIENTNAME_MAX_LENGTH 48	//我们将用户名作为客户端名，最大长度限制
#define MSG_QUEUE_MAX_LENGTH  256	//消息队列的最大长度

//结构体，用于存储client的信息
struct client {
	int occupied;						//表示该是否client对象是否在被使用（我们只有两个对象），0表示未占用，1表示已占用
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
	struct msg_queue_elem *slots[MSG_QUEUE_MAX_LENGTH];			//消息队列里的元素，每个都指向一个content

	volatile int head;			//指向第一个元素 - 由主线程修改
	volatile int tail;			//指向最后一个元素 - 由client线程修改
								//这里使用volatile关键字是为了提醒编译器这两个数据是极易变化的，不能做优化（例如放到寄存器里）

	HANDLE queue_empty_space;		//信号量：指示消息队列还有多少剩余空间
	HANDLE queue_present_message;	//信号量：指示消息队列中还有多少消息
	HANDLE mq_lock;					//信号量：作为互斥锁使用，确保每次只能有一个线程访问消息队列
};