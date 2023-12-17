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


SOCKET server_socket; //�������׽���
SOCKADDR_IN server_addr; //������IP��ַ�Ͷ˿ں�
SOCKADDR_IN client_addr; //�ͻ�����IP��ַ�Ͷ˿ں�
SOCKADDR_IN router_addr; //·����IP�Ͷ˿ں�

pheader sendheader;
pheader recvheader;

int len = sizeof(SOCKADDR); //���յ�ַ�ṹ�ĳ���
string filename;
int windows_len; //���ڴ�С
int filelen;
fstream output; //д����յ����ļ�����

int flag = 0; 
queue<message> storemessage; //һ����Ϣ���У����ڴ洢��Ϣ
message storemsg[10240]; //message ���͵����飬���ڴ洢��Ϣ
int recvflag[10240] = { 0 }; //���ڱ�ǽ���״̬������
message m; // �̴߳�����Ϣ
int expected = 1; // SR������߽�

void printmessage(message a) {
	cout << "��recv��<===[ ";
	if (a.checkSYN())cout << "SYN ";
	if (a.checkACK())cout << "ACK";
	if (a.checkFIN())cout << "FIN ";
	if (a.checkEOF())cout << "EOF ";
	cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" << a.checksum << endl;
}

void printmessage1(message a) {
	cout << "��send��<===[ ";
	if (a.checkSYN())cout << "SYN ";
	if (a.checkACK())cout << "ACK ";
	if (a.checkFIN())cout << "FIN ";
	if (a.checkEOF())cout << "EOF ";
	cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" << a.checksum << endl;
}

//��ʼ���������׽��� �󶨵�ַ
void server_init()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	int flag = WSAStartup(wVersionRequested, &wsaData);
	if (flag != 0) {
		cout << "����ʧ��" << endl;
	}

	server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr.S_un.S_addr);

	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, client_ip, &client_addr.sin_addr.S_un.S_addr);

	router_addr.sin_family = AF_INET;  // ·����ip��port
	router_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, router_ip, &router_addr.sin_addr.S_un.S_addr);

	bind(server_socket, (SOCKADDR*)&server_addr, sizeof(SOCKADDR));

	sendheader.setIP(server_addr.sin_addr.S_un.S_addr, client_addr.sin_addr.S_un.S_addr);
	recvheader.setIP(client_addr.sin_addr.S_un.S_addr, server_addr.sin_addr.S_un.S_addr);
}

void connect() {
	cout << "��ʼ����" << endl;
	message sendmsg;
	message recvmsg;
	//���յ�һ������
	while (1) {
		recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len);
		if (recvmsg.checkSYN() && check(recvmsg, recvheader)) {
			cout << "��һ�����ֳɹ�" << endl;
			break;
		}
	}

	DWORD imode = 1;
	ioctlsocket(server_socket, FIONBIO, &imode);//������
	//���������͵ڶ������ֵ� SYN+ACK ����
	sendmsg.setport(server_port, client_port);
	sendmsg.setSYN();
	sendmsg.setACK();
	sendmsg.setSeq(0);
	sendmsg.setAck(1);
	CheckSum(sendmsg, sendheader);

	sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
	//�������ȴ��ͻ��˵ĵ��������� ACK ����
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
		cout << "���������ֳɹ�" << endl;
	}
	imode = 0;
	ioctlsocket(server_socket, FIONBIO, &imode);//����
}

void disconnect() {
	message sendmsg;
	message recvmsg;

	while (1) {
		recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len);
		if (recvmsg.checkFIN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
			cout << "��һ�λ��ֳɹ�" << endl;
			break;
		}
	}

	DWORD imode = 1;
	ioctlsocket(server_socket, FIONBIO, &imode);//������

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
		cout << "���Ĵλ��ֳɹ�" << endl;
	}
	closesocket(server_socket);
}


//д���߳�
//�����жϵ�ǰ���ڱ߽�ı����Ƿ��Ѿ��յ����յ��򽫱���д���ļ������ҽ������ƶ�һ��λ�ã��ڴ˽����жϡ������򲻶�ѭ���ȴ���
DWORD WINAPI SR_writefiles(LPVOID lpThreadParameter) {
	// ѭ������Ƿ��п�д��ı���
	while (1) {
		// ���������Ŷ�Ӧ�ı����Ƿ��Ѿ�׼����
		if (recvflag[expected] == 1) {
			// ��ȡ������Ŷ�Ӧ�ı���
			m = storemsg[expected];
			expected++; // �����������
			output.write(m.data, m.length); // �����ĵ�����д������ļ�
			if (m.checkEOF()) { // ��鱨���Ƿ������ļ�����
				output.close();
				break;
			}
		}
	}
	return 0;
}

 //�����ļ�
void SR_recvfiles() {

	cout << "��ʼ����" << endl;
	string path = "D:\\GitHub_Koreyoshiy_Files\\Computer_Network_2023fall\\lab3-UDP\\lab3-3\\lab3_3_Sever\\result";
	message sendmsg;
	message recvmsg;

	path = path + "\\" + filename;
	output.open(path, ios::binary | ios::out);

	u_long imode = 0;
	ioctlsocket(server_socket, FIONBIO, &imode);

	//����д���߳�
	//�����жϵ�ǰ���ڱ߽�ı����Ƿ��Ѿ��յ����յ��򽫱���д���ļ������������ƶ�һ��λ�á�
	HANDLE writeproc = CreateThread(nullptr, 0, SR_writefiles, nullptr, 0, nullptr); //

	//���߳� 
	//�����жϵ�ǰ���ڱ߽�ı����Ƿ��Ѿ��յ����յ��򽫱���д���ļ������ҽ������ƶ�һ��λ�ã��ڴ˽����жϡ������򲻶�ѭ���ȴ�

	while (1) {
		//��Ϣ���׽�������
		recvmsg.reset();
		sendmsg.reset();
		//ͨ�� recvfrom �����ӿͻ��˽��ձ��ģ�����洢�� recvmsg ��
		recvfrom(server_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, &len);
		printmessage(recvmsg);
		if (check(recvmsg, recvheader)) {
			if (recvmsg.getSeq() == expected) {
				printf("��   �������   ��");
			}
			else {
				printf("�� ��ʧ����� ����");
			}
			//�жϱ����Ƿ��ڴ�����
			//yes->�洢���ĺ͸��½��ձ�־
			if (recvmsg.getSeq() >= expected && recvmsg.getSeq() < expected + windows_len) {
				storemsg[recvmsg.getSeq()] = recvmsg;
				recvflag[recvmsg.getSeq()] = 1;
			}
			//����ظ�����
			sendmsg.setACK();
			sendmsg.setSeq(recvmsg.getSeq());
			sendmsg.setAck(recvmsg.getSeq());
			if (recvmsg.checkEOF()) { //������ı�����ļ������������ûظ����ĵ��ļ�������־EOF
				sendmsg.setEOF();
			}
			CheckSum(sendmsg, sendheader); //����ָ�����
			printmessage1(sendmsg); //��ӡ�ظ�����
			sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
			if (m.checkEOF()) {
				sendto(server_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&client_addr, len);
				cout << "�˳�" << endl;
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