#include<queue>
#include "message.h"	
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sys/types.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;


#define server_ip "127.0.0.1"
#define server_port 6999
#define client_ip "127.0.0.1"
#define client_port 7001
#define router_ip "127.0.0.1"
#define router_port 7000
#define MSL 10240


SOCKET server_socket; //服务器套接字
SOCKADDR_IN server_addr; //服务器IP地址和端口号
SOCKADDR_IN client_addr; //客户端器IP地址和端口号
SOCKADDR_IN router_addr; //路由器IP和端口号

pheader sendheader;
pheader recvheader;

int len = sizeof(SOCKADDR); //接收地址结构的长度
string filename;
int windows_len; //窗口大小
int filelen;
fstream output; //写入接收到的文件内容

int flag = 0; 
queue<message> storemessage; //一个消息队列，用于存储消息
message storemsg[10240]; //message 类型的数组，用于存储消息
int recvflag[10240] = { 0 }; //用于标记接收状态的数组
message m; // 线程传递消息
int expected = 1; // SR窗口左边界

void printmessage(message a) {
	cout << "【recv】<===[ ";
	if (a.checkSYN())cout << "SYN ";
	if (a.checkACK())cout << "ACK";
	if (a.checkFIN())cout << "FIN ";
	if (a.checkEOF())cout << "EOF ";
	cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" << a.checksum << endl;
}

void printmessage1(message a) {
	cout << "【send】<===[ ";
	if (a.checkSYN())cout << "SYN ";
	if (a.checkACK())cout << "ACK ";
	if (a.checkFIN())cout << "FIN ";
	if (a.checkEOF())cout << "EOF ";
	cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" << a.checksum << endl;
}

//初始化服务器套接字 绑定地址
void server_init()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	int flag = WSAStartup(wVersionRequested, &wsaData);
	if (flag != 0) {
		cout << "启动失败" << endl;
	}

	server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr.S_un.S_addr);

	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, client_ip, &client_addr.sin_addr.S_un.S_addr);

	router_addr.sin_family = AF_INET;  // 路由器ip与port
	router_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, router_ip, &router_addr.sin_addr.S_un.S_addr);

	bind(server_socket, (SOCKADDR*)&server_addr, sizeof(SOCKADDR));

	sendheader.setIP(server_addr.sin_addr.S_un.S_addr, client_addr.sin_addr.S_un.S_addr);
	recvheader.setIP(client_addr.sin_addr.S_un.S_addr, server_addr.sin_addr.S_un.S_addr);
}

void connect() {
	cout << "开始连接" << endl;
	message sendmsg;
	message recvmsg;
	//接收第一次握手
	while (1) {
		recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len);
		if (recvmsg.checkSYN() && check(recvmsg, recvheader)) {
			cout << "第一次握手成功" << endl;
			break;
		}
	}

	DWORD imode = 1;
	ioctlsocket(server_socket, FIONBIO, &imode);//非阻塞
	//服务器发送第二次握手的 SYN+ACK 报文
	sendmsg.setport(server_port, client_port);
	sendmsg.setSYN();
	sendmsg.setACK();
	sendmsg.setSeq(0);
	sendmsg.setAck(1);
	CheckSum(sendmsg, sendheader);

	sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
	//服务器等待客户端的第三次握手 ACK 报文
	recvmsg.reset();
	clock_t begin = clock();
	while (recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len) <= 0) {
		if (clock() - begin > MSL) {
			sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
			begin = clock();
		}
	}

	if (recvmsg.checkACK() && check(recvmsg, recvheader)) {
		filename = recvmsg.data;
		windows_len = recvmsg.length;
		cout << "第三次握手成功" << endl;
	}
	imode = 0;
	ioctlsocket(server_socket, FIONBIO, &imode);//阻塞
}

void disconnect() {
	message sendmsg;
	message recvmsg;

	while (1) {
		recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len);
		if (recvmsg.checkFIN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
			cout << "第一次挥手成功" << endl;
			break;
		}
	}

	DWORD imode = 1;
	ioctlsocket(server_socket, FIONBIO, &imode);//非阻塞

	sendmsg.setport(server_port, client_port);
	sendmsg.setACK();
	CheckSum(sendmsg, sendheader);
	sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);


	sendmsg.reset();
	sendmsg.setport(server_port, client_port);
	sendmsg.setACK();
	sendmsg.setFIN();
	CheckSum(sendmsg, sendheader);
	sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);

	clock_t begin = clock();
	recvmsg.reset();

	while (recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len) <= 0) {
		if (clock() - begin > MSL) {
			sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
			begin = clock();
		}
	}

	if (recvmsg.checkACK() && check(recvmsg, recvheader)) {
		cout << "第四次挥手成功" << endl;
	}
	closesocket(server_socket);
}


//写入线程
//不断判断当前窗口边界的报文是否已经收到，收到则将报文写入文件，并且将窗口移动一个位置，在此进行判断。不是则不断循环等待。
DWORD WINAPI SR_writefiles(LPVOID lpThreadParameter) {
	// 循环检查是否有可写入的报文
	while (1) {
		// 检查期望序号对应的报文是否已经准备好
		if (recvflag[expected] == 1) {
			// 获取期望序号对应的报文
			m = storemsg[expected];
			expected++; // 更新期望序号
			output.write(m.data, m.length); // 将报文的数据写入输出文件
			if (m.checkEOF()) { // 检查报文是否标记了文件结束
				output.close();
				break;
			}
		}
	}
	return 0;
}

 //接收文件
void SR_recvfiles() {

	cout << "开始接收" << endl;
	string path = "D:\\GitHub_Koreyoshiy_Files\\Computer_Network_2023fall\\lab3-UDP\\lab3-3\\lab3_3_Sever\\result";
	message sendmsg;
	message recvmsg;

	path = path + "\\" + filename;
	output.open(path, ios::binary | ios::out);

	u_long imode = 0;
	ioctlsocket(server_socket, FIONBIO, &imode);

	//创建写入线程
	//不断判断当前窗口边界的报文是否已经收到，收到则将报文写入文件，并将窗口移动一个位置。
	HANDLE writeproc = CreateThread(nullptr, 0, SR_writefiles, nullptr, 0, nullptr); //

	//主线程 
	//不断判断当前窗口边界的报文是否已经收到，收到则将报文写入文件，并且将窗口移动一个位置，在此进行判断。不是则不断循环等待

	while (1) {
		//消息和套接字重置
		recvmsg.reset();
		sendmsg.reset();
		//通过 recvfrom 函数从客户端接收报文，将其存储在 recvmsg 中
		recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len);
		printmessage(recvmsg);
		if (check(recvmsg, recvheader)) {
			if (recvmsg.getSeq() == expected) {
				printf("【   按序接收   】");
			}
			else {
				printf("【 ！失序接收 ！】");
			}
			//判断报文是否在窗口内
			//yes->存储报文和更新接收标志
			if (recvmsg.getSeq() >= expected && recvmsg.getSeq() < expected + windows_len) {
				storemsg[recvmsg.getSeq()] = recvmsg;
				recvflag[recvmsg.getSeq()] = 1;
			}
			//构造回复报文
			sendmsg.setACK();
			sendmsg.setSeq(recvmsg.getSeq());
			sendmsg.setAck(recvmsg.getSeq());
			if (recvmsg.checkEOF()) { //如果报文标记了文件结束，则设置回复报文的文件结束标志EOF
				sendmsg.setEOF();
			}
			CheckSum(sendmsg, sendheader); //检验恢复报文
			printmessage1(sendmsg); //打印回复报文
			sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
			if (m.checkEOF()) {
				sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
				cout << "退出" << endl;
				break;
			}
		}
		cout << endl;
	}
}

int main() {
	server_init();
	connect();
	SR_recvfiles();
	disconnect();
	return 0;
}