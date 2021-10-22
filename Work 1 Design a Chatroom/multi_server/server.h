#pragma once
#include <Windows.h>
#include <time.h>

#define CLIENTNAME_MAX_LENGTH 48	//���ǽ��û�����Ϊ�ͻ���������󳤶�����
#define MSG_QUEUE_MAX_LENGTH  256	//��Ϣ���е���󳤶�
#define CLIENT_QUEUE_MAX_NUM  24	//���������û�����
#define BACKLOG 10					//listen�ȴ����е���󳤶�

//�ṹ�壬���ڴ洢client����Ϣ��ͬʱҲ��client_queue�����Ԫ��
struct client {
	struct client* prev, * next;		//ָ��client���е�ǰ��Ԫ��
	SOCKET socket;						//�洢client��socket��Ϣ
	struct sockaddr_in address;			//�洢Զ��client�ĵ�ַ��Ϣ
	char client_name[CLIENTNAME_MAX_LENGTH]; //�洢�ͻ����ƣ����û�����
	HANDLE client_thread;				//����һ���ͻ����̣߳����ڽ����û���Ϣ����������Ϣ������
};

//��Ϣ�������Ԫ��
struct msg_queue_elem {
	time_t timep;
	int content_length;
	char content[0];
};

//��Ϣ���У�
//��������ά����һ���൱��Ҫ�����ݽṹ�����ڴ��client�߳̽��ܴ�������Ϣ
//��Ϣ���е�Ԫ�ض���ָ���ض��ڴ������߳��������Ķ�������ָ��
//��Ϣ����ʹ���ź������в������ƣ���ֹ��ͻ
//��Ϣ���в���FIFO����������Ϣ����
struct chatmsg_queue {
	struct msg_queue_elem* slots[MSG_QUEUE_MAX_LENGTH];			//��Ϣ�������Ԫ�أ�ÿ����ָ��һ��content

	volatile int head;			//ָ���һ��Ԫ�� - �����߳��޸�
	volatile int tail;			//ָ�����һ��Ԫ�� - ��client�߳��޸�
								//����ʹ��volatile�ؼ�����Ϊ�����ѱ����������������Ǽ��ױ仯�ģ��������Ż�������ŵ��Ĵ����

	HANDLE queue_empty_space;		//�ź�����ָʾ��Ϣ���л��ж���ʣ��ռ�
	HANDLE queue_present_message;	//�ź�����ָʾ��Ϣ�����л��ж�����Ϣ
	HANDLE mq_lock;					//�ź�������Ϊ������ʹ�ã�ȷ��ÿ��ֻ����һ���̷߳�����Ϣ����
};

//�û����У�
//������ά����һ��ѭ��˫���������ڴ��ע�����û�
struct client_queue {
	volatile int count;				//��¼�������ж���Ԫ��
	struct client* head, * tail;	//ָ����е�ͷ��β
	HANDLE cq_lock;					//�ź�����ʵ�ֻ�����ʣ�ͬһʱ��ֻ����һ���˷��ʶ���
};