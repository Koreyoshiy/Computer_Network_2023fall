#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include<iostream>
#include<ctime>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

#define RECV_OVER 1    //已收到
#define RECV_YET 0     //还没收到
char userName[16] = { 0 };
boolean isPrint = false;  // 判断是否要在客户端打印名字
int cStatus = RECV_YET;

//接收数据线程
unsigned __stdcall ThreadRecv(void* param)
{
	char bufferRecv[128] = { 0 };
	while (true)
	{
		int error = recv(*(SOCKET*)param, bufferRecv, sizeof(bufferRecv), 0);
		if (error == SOCKET_ERROR)
		{
			Sleep(500);
			continue;
		}
		if (strlen(bufferRecv) != 0)
		{
			if (isPrint)
			{
				for (unsigned int i = 0; i <= strlen(userName) + 2; i++)
					cout << "\b";
				cout << bufferRecv << endl;
				//printf("\r");    //覆盖之前的名字
				//printf("%s\n", bufferRecv);
				cout << userName << ": ";   //因为这是在用户的send态时，把本来打印出来的userName给退回去了，所以收到以后需要再把userName打印出来
				cStatus = RECV_OVER;
			}
			else
				cout << bufferRecv << endl;
		}
		else
			Sleep(100);
	}
	return 0;
}

string getCurrentTime() {
	time_t now = time(0);
	struct tm timeinfo;
	char buffer[10];
	localtime_s(&timeinfo, &now);
	strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
	return string(buffer);
}

int main()
{
	//// 获取当前时间
	//time_t now = time(0);
	//// 转换为本地时间
	//tm* ltm = localtime(&now);

	WSADATA wsaData = { 0 };//存放套接字信息
	SOCKET ClientSocket = INVALID_SOCKET;//客户端套接字
	SOCKADDR_IN ServerAddr = { 0 };//服务端地址
	USHORT uPort = 1666;//服务端端口
						 //初始化套接字
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))//将库文件和程序绑定
	{
		cout << "WSAStartup error: " << WSAGetLastError() << endl;
		return -1;
	}

	//创建套接字
	ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ClientSocket == INVALID_SOCKET)
	{
		cout << "Socket error: " << WSAGetLastError() << endl;
		return -1;
	}

	//输入服务器IP
	/*cout << "Please input the server IP:";
	char IP[32] = { 0 };
	cin.getline(IP,32);*/


	//设置服务器地址
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(uPort);//服务器端口
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//服务器地址

	cout << "正在连接，请稍等……" << endl;

	//连接服务器
	if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "Connect error: " << WSAGetLastError() << endl;
		closesocket(ClientSocket);
		WSACleanup();
		return -1;
	}

	cout << "连接成功！" << "服务器IP是:" << inet_ntoa(ServerAddr.sin_addr);
	cout << "服务器端口是:" << htons(ServerAddr.sin_port) << endl << endl;
	cout << "请输入你的用户名: ";

	cin.getline(userName, 16);
	cout << "关闭程序请输入‘exit’ " << endl;
	send(ClientSocket, userName, sizeof(userName), 0);
	cout << endl;

	char identify[16] = { 0 };
	cout << "可以选择其他用户进行聊天。如果你输入'all'，那么每个人都可以收到你的信息；如果你输入用户的名字，那么只有这个用户可以收到你的信息 " << endl;
	cout << "请选择你要聊天的用户: ";
	cin.getline(identify, 16);
	send(ClientSocket, identify, sizeof(identify), 0);

	_beginthreadex(NULL, 0, ThreadRecv, &ClientSocket, 0, NULL); //启动接收线程

	char bufferSend[128] = { 0 };

	while (1)
	{
		if (isPrint == false)
		{
			// 获取当前时间
			time_t now = time(0);
			// 转换为本地时间
			tm* ltm = localtime(&now);

			//cout << getCurrentTime()<<userName  << ": ";
			cout << ltm->tm_hour << ":" << ltm->tm_min << " " << userName << ":";
			isPrint = true;
		}
		cin.getline(bufferSend, 128);
		if (strcmp(bufferSend, "exit") == 0)
		{
			cout << "正在退出..." << endl;
			Sleep(2000);
			int error = send(ClientSocket, bufferSend, sizeof(bufferSend), 0);
			if (error == SOCKET_ERROR)
				return -1;    //退出当前线程
			return 0;   //线程会关闭
		}

		int error = send(ClientSocket, bufferSend, sizeof(bufferSend), 0);
		if (error == SOCKET_ERROR)
			return -1;    //退出当前线程
		if (error != 0)
		{
			isPrint = false;
		}
	}
	for (int k = 0; k < 1000; k++)    //让主线程一直转
		Sleep(10000000);

	closesocket(ClientSocket);
	WSACleanup();
	return 0;
}