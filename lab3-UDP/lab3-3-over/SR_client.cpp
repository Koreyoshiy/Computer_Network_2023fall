#include<iostream>
#include<string>
#include<fstream>
#include<time.h>
#include<process.h>
#include "message.h"	
#include<windows.h>
#include <mutex>
using namespace std;

#define server_ip "127.0.0.1"
#define server_port 6999
#define client_ip "127.0.0.1"
#define client_port 7001
#define router_ip "127.0.0.1"
#define router_port 7000

#define MSL 500
int windows_len;

std::mutex mtx;

SOCKET client_socket;
SOCKADDR_IN server_addr;
SOCKADDR_IN client_addr;
SOCKADDR_IN router_addr;

pheader sendheader;
pheader recvheader;

message recvmsg1; // 主线程与接收线程传递消息

int len = sizeof(SOCKADDR);

string filename;
int filelen;
char* file;

clock_t sendstart, sendend; // 计算吞吐率的时间
clock_t sendstart1, sendend1;
int flag = 0;

int startid, nextid, maxrecv, maxsent = 0; // startid:窗口左边界,nextid:窗口右边界,maxrecv:累计确认的编号,maxsent:已发送数据包的编号
clock_t SRbegin;// SR超时重传
int alen;  // 文件分组数目
bool resent = false;

bool* recvACK; // 确认接收数组

void printmessage(message a) {
	/*cout << "【send】<===[ ";
	if (a.checkSYN())cout << "SYN ";
	if (a.checkACK())cout << "ACK ";
	if (a.checkFIN())cout << "FIN ";
	if (a.checkEOF())cout << "EOF ";
	mtx.lock();
	cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" <<a.checksum << endl;
	mtx.unlock();*/
	printf("【send】<===[");
	if (a.checkSYN())printf("SYN ");
	if (a.checkACK())printf("ACK ");
	//if (!(a.checkACK()))cout << "ACK(0) ";
	if (a.checkFIN())printf("FIN ");
	if (a.checkEOF())printf("EOF ");
	printf("], seq=%d, ack=%d, len=%d, checksum=%d\n", a.getSeq(), a.getAck(), a.length, a.checksum);
}

void printmessage1(message a) {
	//cout << "【recv】<===[ ";
	//if (a.checkSYN())cout << "SYN ";
	//if (a.checkACK())cout << "ACK";
	////if (!(a.checkACK()))cout << "ACK(0) ";
	//if (a.checkFIN())cout << "FIN ";
	//if (a.checkEOF())cout << "EOF ";
	//mtx.lock();
	//cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" << a.checksum << endl;
	//mtx.unlock();
	printf("【recv】<===[");
	if (a.checkSYN())printf("SYN ");
	if (a.checkACK())printf("ACK");
	//if (!(a.checkACK()))cout << "ACK(0) ";
	if (a.checkFIN())printf("FIN ");
	if (a.checkEOF())printf("EOF ");
	printf("], seq=%d, ack=%d, len=%d, checksum=%d\n", a.getSeq(), a.getAck(), a.length, a.checksum);
}

void client_init()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	int flag = WSAStartup(wVersionRequested, &wsaData);
	if (flag != 0) {
		cout << "启动失败" << endl;
	}

	client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	server_addr.sin_family = AF_INET;  // 服务器ip与port
	server_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr.S_un.S_addr);

	client_addr.sin_family = AF_INET;  // 客户端ip与port
	client_addr.sin_port = htons(client_port);
	inet_pton(AF_INET, client_ip, &client_addr.sin_addr.S_un.S_addr);

	router_addr.sin_family = AF_INET;  // 路由器ip与port
	router_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, router_ip, &router_addr.sin_addr.S_un.S_addr);

	bind(client_socket, (SOCKADDR*)&client_addr, sizeof(SOCKADDR));

	sendheader.setIP(client_addr.sin_addr.S_un.S_addr, server_addr.sin_addr.S_un.S_addr);
	recvheader.setIP(server_addr.sin_addr.S_un.S_addr, client_addr.sin_addr.S_un.S_addr);
}

void connect() {
	cout << "开始连接" << endl;
	message sendmsg;
	message recvmsg;

	sendmsg.setport(client_port, server_port);
	sendmsg.setSYN();
	CheckSum(sendmsg, sendheader);

	DWORD imode = 1;
	ioctlsocket(client_socket, FIONBIO, &imode);//非阻塞
	//发送第一次握手
	sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
	clock_t begin = clock();
	//接收第二次握手
	while (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len) <= 0) {
		if (clock() - begin > MSL) { //超过500ms进行重传
			sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
			begin = clock();
		}
	}

	if (recvmsg.checkSYN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
		cout << "第二次握手成功" << endl;
		sendmsg.reset();
		sendmsg.setport(client_port, server_port);
		sendmsg.setACK();
		sendmsg.setSeq(1);
		sendmsg.setAck(1);
		sendmsg.setlen(windows_len);
		memcpy(sendmsg.data, filename.c_str(), filename.length());
		CheckSum(sendmsg, sendheader);
		//发送第三次握手
		sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
		begin = clock();

		while (1) {
			if (clock() - begin >= 2 * MSL) { 
				break;
			}
			if (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len) > 0) {
				if (recvmsg.checkFIN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
					sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
					begin = clock();
				}
			}
		}
	}
	else {
		cout << "第二次握手失败" << endl;
	}
}

void disconnect() {
	message sendmsg;
	message recvmsg;
	sendmsg.setport(client_port, server_port);
	sendmsg.setFIN();
	sendmsg.setACK();
	CheckSum(sendmsg, sendheader);

	sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
	clock_t begin = clock();


	while (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len) <= 0) {
		if (clock() - begin > MSL) {
			sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
			begin = clock();
		}
	}

	if (recvmsg.checkACK() && check(recvmsg, recvheader)) {
		recvmsg.reset();
		cout << "第二次挥手成功" << endl;
	}

	while (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len)) {
		if (recvmsg.checkFIN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
			cout << "第三次挥手成功" << endl;
			break;
		}
	}

	sendmsg.reset();
	sendmsg.setport(client_port, server_port);
	sendmsg.setACK();
	CheckSum(sendmsg, sendheader);

	sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);

	begin = clock();
	while (1) {
		if (clock() - begin >= 2 * MSL) {
			break;
		}
		if (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len) > 0) {
			if (recvmsg.checkFIN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
				sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
				begin = clock();
			}
		}
	}

	closesocket(client_socket);
	return;
}

void readfiles() {
	string path = "D:\\GitHub_Koreyoshiy_Files\\Computer_Network_2023fall\\lab3-UDP\\lab3-3\\lab3_3_Client\\test";
	path += "\\" + filename;
	ifstream fin;
	fin.open(path, ios::binary);
	if (!fin) {
		cout << "打开文件失败" << endl;
	}
	//定位回文件开头
	fin.seekg(0, ios::end);
	filelen = fin.tellg();
	fin.seekg(0, ios::beg);
	file = new char[filelen];
	fin.read(file, filelen);
	fin.close();
}


//接收线程
//循环接收报文
/*当接受报文的序落在窗口内的时候，将报文标记为已接收。
如果接受到的报文序号是窗口中最小序号，则将窗口进行滑动，滑动到下一个未确认接收的报文处。
具体是通过 while 循环检测标号是 startid 的 报文是否已经被接收。
如果是，讲过窗口滑动一个位置继续检测，不是则退出循环。*/
DWORD WINAPI SR_recvmessage(LPVOID lpThreadParameter) {
	message recvmsg;
	u_long imode = 0;
	ioctlsocket(client_socket, FIONBIO, &imode);
	while (1) {
		//接收消息
		recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len);
		printmessage1(recvmsg);
		// 检查是否是确认消息
		if (recvmsg.checkACK() && check(recvmsg, recvheader)) {
			// 如果收到的 ACK 序号大于等于窗口左边界（startid）
			if (startid <= recvmsg.getAck()) {
				recvACK[recvmsg.getAck()] = true; //标记为已接收
				// 如果窗口左边界等于已发送的最大序号 + 1，则继续
				if (startid == maxsent + 1) {
					continue;
				}
				else {
					// 否则，更新 SR 超时计时器
					SRbegin = clock();
				}
				while (1) {
					// 检查窗口内的报文是否已经被确认，如果是，更新窗口左边界
					if (recvACK[startid] && startid <= alen) {
						startid++;
						SRbegin = clock();
					}
					else {
						break;
					}
				}
				mtx.lock();
				//printmessage1(recvmsg);
				printf("窗口左边界=%d, 窗口右边界=%d\n", startid, (((startid + windows_len) <= alen) ? (startid + windows_len) : alen + 1));
				printf("\n");
				mtx.unlock();
			}
			// 如果窗口左边界等于文件总分组数 + 1，说明已经接收完所有分组，结束循环
			if (startid == alen + 1) {
				break;
			}
		}
	}
	return 1;
}


//重发线程
/*重发线程是否工作受 resent 变量控制。
当 resent 为 true 的时候，重发窗口内所有发送但未确认的报文。*/
DWORD WINAPI SR_resentmessage(LPVOID lpThreadParameter) {
	message sendmsg;
	while (1) {
		//如果允许重发
		if (resent) {
			// 遍历窗口内的所有分组
			for (int i = startid; i <= maxsent; i++) {
				// 如果分组已经确认接收，则跳过
				if (recvACK[i]) {
					continue;
				}
				// 构造要重发的消息
				sendmsg.reset();
				sendmsg.setport(client_port, server_port);
				if (i == alen) {
					// 判断是否是最后一个分组
					sendmsg.setEOF(); // 设置消息为文件结束标志
					if (filelen % max_size != 0) {
						// 如果文件长度不是 max_size 的整数倍
						// 复制文件的剩余部分到消息的数据字段
						memcpy(sendmsg.data, file + (i - 1) * max_size, filelen % max_size);
					}
					else {
						// 否则，复制 max_size 长度的数据到消息的数据字段
						memcpy(sendmsg.data, file + (i - 1) * max_size, max_size); // 设置消息的数据长度为剩余部分的长度
					}
					sendmsg.setlen(filelen % max_size);
				}
				else { // 如果不是最后一个分组
					// 复制 max_size 长度的数据到消息的数据字段
					memcpy(sendmsg.data, file + (i - 1) * max_size, max_size);
					// 设置消息的数据长度为 max_size
					sendmsg.setlen(max_size);
				}
				sendmsg.setSeq(i);// 设置消息的序号为 i
				sendmsg.setAck(i);
				CheckSum(sendmsg, sendheader); // 设置消息的序号为 i
				// 打印消息信息，如果是重发状态，输出重发标志
				mtx.lock();
				if (resent) {
					printf("【重发！！！】");
				}
				printmessage(sendmsg);
				printf("\n");
				mtx.unlock();
				// 发送重发消息
				sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
			}
			// 重发完成后将 resent 标记为 false
			resent = false;
		}
	}
}

//发送文件
void SR_sendfiles() {
	cout << "开始传输" << endl;
	//计算alen分组数目，max_size为每个分组的大小
	alen = int(filelen / max_size);
	if (filelen % max_size != 0) {
		alen += 1;
	}
	//初始化接收确认数组（recvACK）记录每个分组是否已经接收到确认
	recvACK = new bool[alen + 1];
	for (int i = 0; i < alen + 1; i++) {
		recvACK[i] = false;
	}

	message sendmsg; //发送消息
	startid = 1;
	maxrecv = 0;
	//recvproc 用于接收确认消息，resentproc 用于处理重发
	HANDLE recvproc = CreateThread(nullptr, 0, SR_recvmessage, nullptr, 0, nullptr);
	HANDLE resentproc = CreateThread(nullptr, 0, SR_resentmessage, nullptr, 0, nullptr);

	//记录传输开始的时间
	sendstart1 = clock();

	//主线程
	//每次循环中发送当前窗口中的未被发送的内容。
	// 用 startid 表示当前窗口的左边界，用nextid 表示当前窗口的右边界，用 maxsent 表示已发送报文的编号。
	//for循环将当前窗口中的所有未发送的报文进行发送。每次发送一个报文更新一次 maxsent 。
	//如果超时，则将变量 resent 设置为 true ，此变量会控制重发线程是否重发消息。
	while (1) {
		//计算当前窗口右边界nextid  startid是左边界 
		//如果窗口左边界加上窗口大小小于等于数据包总数，就把 nextid 设置为左边界加上窗口大小，否则设置为总数据包数。
		nextid = ((startid + windows_len) <= alen) ? (startid + windows_len) : alen + 1;
		//maxsent已发送的数据包编号 发送maxsent至nextid消息
		for (int i = maxsent + 1; (i < nextid); i++) {
			//构造发送消息
			sendmsg.reset();//重置发送消息对象
			sendmsg.setport(client_port, server_port);
			if (i == alen) { //如果i遍历到最后一个数据包，flag设为EOF并复制文件数据到消息中
				sendmsg.setEOF();
				if (filelen % max_size != 0) {
					memcpy(sendmsg.data, file + (i - 1) * max_size, filelen % max_size);
				}
				else {
					memcpy(sendmsg.data, file + (i - 1) * max_size, max_size);
				}
				sendmsg.setlen(filelen % max_size);
			}
			else {
				memcpy(sendmsg.data, file + (i - 1) * max_size, max_size);
				sendmsg.setlen(max_size);
			}
			sendmsg.setSeq(i);
			sendmsg.setAck(i);
			CheckSum(sendmsg, sendheader);
			mtx.lock();
			printmessage(sendmsg);
			printf("\n");
			mtx.unlock();
			sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
			maxsent = i; //更新
			if (startid == maxsent) { //如果窗口的左边界等于最大发送编号，表示窗口滑动，更新 SR 超时计时器 SRbegin
				SRbegin = clock();
			}
		}
		if (startid == alen + 1) { //已发送完成，退出循环
			break;
		}
		if (clock() - SRbegin > MSL) { //超时 resent设为TRUE 需重传
			SRbegin = clock();
			resent = true;
		}
	}
	//记录传输结束时间
	sendend1 = clock();
	double ti = (((double)sendend1 - (double)sendstart1) / 1000);
	printf("重传延时：%d ms\n窗口大小:%d\n传输时延：%f s\n吞吐率：%f byte/s\n", MSL, windows_len, ti, (filelen / ti));
	
}

int main() {
	cout << "输入测试文件" << endl;
	string v;
	cin >> v;
	filename = v;
	cout << "请输入窗口大小" << endl;
	cin >> windows_len;
	client_init();
	connect();
	readfiles();
	SR_sendfiles();
	disconnect();
	return 0;
}