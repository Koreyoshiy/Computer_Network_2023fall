#include <WinSock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include <ctime>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

#define SEND_OVER 1							 //消息已转发
#define SEND_YET  0							 //消息未转发
int i = 0;//跟踪在线客户端的数量
int Status = SEND_YET;//信息发送状态，初始为未转发

SOCKET ServerSocket = INVALID_SOCKET;		 //服务端套接字，接受客户端的连接
                                             //=INVALID_SOCKET 表示服务器套接字尚未被有效地初始化。
SOCKADDR_IN ClientAddr = { 0 };			 //客户端地址
int ClientAddrLen = sizeof(ClientAddr);//客户端地址长度

bool CheckConnect = false;                //检查连接情况
HANDLE HandleRecv[10] = { NULL };				 //数组，存储线程句柄，每个句柄对应一个客户端的接收线程
HANDLE mainHandle;							 //用于accept的线程句柄（用于处理接受客户端连接请求的线程）

//客户端信息结构体
typedef struct _Client
{
	SOCKET sClient;      //客户端套接字
	char buffer[128];		 //数据缓冲区
	char userName[16];   //客户端用户名
	char identify[16];   //用于标识转发的范围
	char IP[20];		 //客户端IP
	UINT_PTR flag;       //标记客户端，用来区分不同的客户端
} Client;

Client insideClient[10] = { 0 };                  //创建一个客户端结构体,最多同时10人在线

//发送数据线程
unsigned __stdcall ThreadSend(void* param)
{
	int error = 0;
	int flag = *(int*)param;//将传递给线程的参数 param 转换为整数，并将其赋值给 flag。flag 表示要发送消息的客户端的标志
	SOCKET client = INVALID_SOCKET;					//创建一个临时套接字来存放要转发的客户端套接字
	char temp[128] = { 0 };							//创建一个临时的数据缓冲区，用来存放接收到的数据
	memcpy(temp, insideClient[flag].buffer, sizeof(temp));//将 insideClient[flag].buffer 中的消息复制到 temp 中，以备后续修改和发送。
	sprintf_s(insideClient[flag].buffer, "%s: %s", insideClient[flag].userName, temp); //把发送源的名字添进转发的信息里
	if (strlen(temp) != 0 && Status == SEND_YET) //如果数据不为空且还没转发则转发
	{
		if (strcmp(insideClient[flag].identify, "all") == 0)//如果是all就转发给其他所有人
		{
			for (int j = 0; j < i; j++) {
				if (j != flag) {//向除自己之外的所有客户端发送信息
					error = send(insideClient[j].sClient, insideClient[flag].buffer, sizeof(insideClient[j].buffer), 0);
					if (error == SOCKET_ERROR)
						return 1;
				}
			}
		}
		else//否则发给指定的那个人
		{
			for (int j = 0; j < i; j++)
				if (strcmp(insideClient[j].userName, insideClient[flag].identify) == 0)
				{
					error = send(insideClient[j].sClient, insideClient[flag].buffer, sizeof(insideClient[j].buffer), 0);
					if (error == SOCKET_ERROR)
						return 1;
				}
		}
	}

	Status = SEND_OVER;   //转发成功后设置状态为已转发
	return 0;
}

//接受数据线程
unsigned __stdcall ThreadRecv(void* param)
{
	//// 获取当前时间
	//time_t now = time(0);
	//// 转换为本地时间
	//tm* ltm = localtime(&now);


	SOCKET client = INVALID_SOCKET;
	int flag = 0;//标记要处理的客户端的索引
	for (int j = 0; j < i; j++) {
		if (*(int*)param == insideClient[j].flag)            //判断是为哪个客户端开辟的接收数据线程
		{
			client = insideClient[j].sClient;
			flag = j;
		}
	}
	char temp[128] = { 0 };  //临时数据缓冲区
	while (true)//该循环用于不断接收来自客户端的消息
	{
		// 获取当前时间
		time_t now = time(0);
		// 转换为本地时间
		tm* ltm = localtime(&now);

		memset(temp, 0, sizeof(temp));//将 temp 数组的内容全部初始化为零，以确保缓冲区为空。
		int error = recv(client, temp, sizeof(temp), 0); //接收数据
		if (error == SOCKET_ERROR)
			continue;
		Status = SEND_YET;	//设置转发状态为未转发
		memcpy(insideClient[flag].buffer, temp, sizeof(insideClient[flag].buffer));/*将接收到的消息从 temp 缓冲区复制到
																				   相应客户端的消息缓冲区 insideClient[flag].buffer 中*/
		if (strcmp(temp, "exit") == 0)   //判断如果客户发送exit请求，那么直接关闭线程，不打开转发线程
		{
			closesocket(insideClient[flag].sClient);//关闭该套接字
			CloseHandle(HandleRecv[flag]); //这里关闭了线程句柄
			insideClient[flag].sClient = 0;  //把这个位置空出来，留给以后进入的线程使用
			HandleRecv[flag] = NULL;
			//用printf来取参数
			cout << ltm->tm_hour << ":" << ltm->tm_min<<  "【" <<insideClient[flag].IP << "】" << "'" << insideClient[flag].userName<< "'" << "has disconnected from the server. " << endl;
		}
		else
		{
			cout << ltm->tm_hour << ":" << ltm->tm_min << "【" << insideClient[flag].IP << "】" << insideClient[flag].userName << ": " << temp << endl;
			_beginthreadex(NULL, 0, ThreadSend, &flag, 0, NULL); //开启一个转发线程,flag标记着这个需要被转发的信息是从哪个线程来的；
		}
	}
	return 0;
}


//接受请求
unsigned __stdcall MainAccept(void* param)
{
	//// 获取当前时间
	//time_t now = time(0);
	//// 转换为本地时间
	//tm* ltm = localtime(&now);

	int flag[10] = { 0 };
	while (true)//监听和接收客户端的连接请求
	{
		if (insideClient[i].flag != 0)   //找到从前往后第一次没被连接的insideClient
		{
			i++;
			continue;
		}
		//如果有客户端申请连接就接受连接
		if ((insideClient[i].sClient = accept(ServerSocket, (SOCKADDR*)&ClientAddr, &ClientAddrLen)) == INVALID_SOCKET)
		{
			cout << "Accept error: " << WSAGetLastError() << endl;
			closesocket(ServerSocket);
			WSACleanup();
			return -1;
		}
		else if (i > 10) {
			cout << "连接失败，人数已超上限！" << endl;
			closesocket(ServerSocket);
			WSACleanup();
			return -1;
		}
		recv(insideClient[i].sClient, insideClient[i].userName, sizeof(insideClient[i].userName), 0); //接收用户名
		recv(insideClient[i].sClient, insideClient[i].identify, sizeof(insideClient[i].identify), 0); //接收转发的范围
		
		memcpy(insideClient[i].IP, inet_ntoa(ClientAddr.sin_addr), sizeof(insideClient[i].IP)); //记录客户端IP

		// 获取当前时间
		time_t now = time(0);
		// 转换为本地时间
		tm* ltm = localtime(&now);

		cout << ltm->tm_hour << ":" << ltm->tm_min<< "The user: '" << insideClient[i].userName << "'" << "【" << insideClient[i].IP << "】"  << " connect successfully!"  << endl;
		//memcpy(insideClient[i].IP, inet_ntoa(ClientAddr.sin_addr), sizeof(insideClient[i].IP)); //记录客户端IP
		insideClient[i].flag = insideClient[i].sClient; //不同的socke有不同UINT_PTR类型的数字来标识
		i++;    //如果去掉这个最开始的赋值会报错
		for (int j = 0; j < i; j++)
		{
			if (insideClient[j].flag != flag[j])
			{
				if (HandleRecv[j])   //如果那个句柄已经被清零了，那么那就关掉那个句柄
					CloseHandle(HandleRecv[j]);
				HandleRecv[j] = (HANDLE)_beginthreadex(NULL, 0, ThreadRecv, &insideClient[j].flag, 0, NULL); //开启接收消息的线程
			}
		}

		for (int j = 0; j < i; j++)
		{
			flag[j] = insideClient[j].flag;//防止ThreadRecv线程多次开启
		}
		Sleep(3000);
	}
	return 0;
}

int main()
{
	WSADATA wsaData = { 0 };

	//初始化套接字
	//指定要使用的 Winsock 版本2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "WSAStartup error: " << WSAGetLastError() << endl;
		return -1;
	}

	//创建套接字
	ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ServerSocket == INVALID_SOCKET)
	{
		cout << "Socket error: " << WSAGetLastError() << endl;
		return -1;
	}

	SOCKADDR_IN ServerAddr = { 0 };				//服务端地址
	USHORT uPort = 1666;						//服务器监听端口
	//设置服务器地址
	ServerAddr.sin_family = AF_INET;//连接方式
	ServerAddr.sin_port = htons(uPort);//服务器监听端口
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//任何客户端都能连接这个服务器

	//绑定服务器
	if (SOCKET_ERROR == bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "Bind error: " << WSAGetLastError() << endl;
		closesocket(ServerSocket);
		return -1;
	}

	//设置监听客户端连接数
	if (SOCKET_ERROR == listen(ServerSocket, 20))
	{
		cout << "Listen error: " << WSAGetLastError() << endl;
		closesocket(ServerSocket);
		WSACleanup();
		return -1;
	}

	cout << "等待用户连接..." << endl;
	mainHandle = (HANDLE)_beginthreadex(NULL, 0, MainAccept, NULL, 0, 0);   //mainHandle是连接其他client的主要线程
	char Serversignal[10];
	cout << "关闭服务器请输入'exit' " << endl;
	while (true)
	{
		cin.getline(Serversignal, 10);
		if (strcmp(Serversignal, "exit") == 0)
		{
			cout << "服务器已关闭. " << endl;
			CloseHandle(mainHandle);
			for (int j = 0; j < i; j++) //依次关闭套接字
			{
				if (insideClient[j].sClient != INVALID_SOCKET)
					closesocket(insideClient[j].sClient);
			}
			closesocket(ServerSocket);
			WSACleanup();
			exit(1);
			return 1;
		}
	}
	return 0;
}