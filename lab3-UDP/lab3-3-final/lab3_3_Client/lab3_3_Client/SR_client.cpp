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

message recvmsg1; // ���߳�������̴߳�����Ϣ

int len = sizeof(SOCKADDR);

string filename;
int filelen;
char* file;

clock_t sendstart, sendend; // ���������ʵ�ʱ��
clock_t sendstart1, sendend1;
int flag = 0;

int startid, nextid, maxrecv, maxsent = 0; // startid:������߽�,nextid:�����ұ߽�,maxrecv:�ۼ�ȷ�ϵı��,maxsent:�ѷ������ݰ��ı��
clock_t SRbegin;// SR��ʱ�ش�
int alen;  // �ļ�������Ŀ
bool resent = false;

bool* recvACK; // ȷ�Ͻ�������

void printmessage(message a) {
	/*cout << "��send��<===[ ";
	if (a.checkSYN())cout << "SYN ";
	if (a.checkACK())cout << "ACK ";
	if (a.checkFIN())cout << "FIN ";
	if (a.checkEOF())cout << "EOF ";
	mtx.lock();
	cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" <<a.checksum << endl;
	mtx.unlock();*/
	printf("��send��<===[");
	if (a.checkSYN())printf("SYN ");
	if (a.checkACK())printf("ACK ");
	//if (!(a.checkACK()))cout << "ACK(0) ";
	if (a.checkFIN())printf("FIN ");
	if (a.checkEOF())printf("EOF ");
	printf("], seq=%d, ack=%d, len=%d, checksum=%d\n", a.getSeq(), a.getAck(), a.length, a.checksum);
}

void printmessage1(message a) {
	//cout << "��recv��<===[ ";
	//if (a.checkSYN())cout << "SYN ";
	//if (a.checkACK())cout << "ACK";
	////if (!(a.checkACK()))cout << "ACK(0) ";
	//if (a.checkFIN())cout << "FIN ";
	//if (a.checkEOF())cout << "EOF ";
	//mtx.lock();
	//cout << "]" << ",seq=" << a.getSeq() << ",ack=" << a.getAck() << ",len=" << a.length << ",checksum=" << a.checksum << endl;
	//mtx.unlock();
	printf("��recv��<===[");
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
		cout << "����ʧ��" << endl;
	}

	client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	server_addr.sin_family = AF_INET;  // ������ip��port
	server_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr.S_un.S_addr);

	client_addr.sin_family = AF_INET;  // �ͻ���ip��port
	client_addr.sin_port = htons(client_port);
	inet_pton(AF_INET, client_ip, &client_addr.sin_addr.S_un.S_addr);

	router_addr.sin_family = AF_INET;  // ·����ip��port
	router_addr.sin_port = htons(router_port);
	inet_pton(AF_INET, router_ip, &router_addr.sin_addr.S_un.S_addr);

	bind(client_socket, (SOCKADDR*)&client_addr, sizeof(SOCKADDR));

	sendheader.setIP(client_addr.sin_addr.S_un.S_addr, server_addr.sin_addr.S_un.S_addr);
	recvheader.setIP(server_addr.sin_addr.S_un.S_addr, client_addr.sin_addr.S_un.S_addr);
}

void connect() {
	cout << "��ʼ����" << endl;
	message sendmsg;
	message recvmsg;

	sendmsg.setport(client_port, server_port);
	sendmsg.setSYN();
	CheckSum(sendmsg, sendheader);

	DWORD imode = 1;
	ioctlsocket(client_socket, FIONBIO, &imode);//������
	//���͵�һ������
	sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
	clock_t begin = clock();
	//���յڶ�������
	while (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len) <= 0) {
		if (clock() - begin > MSL) { //����500ms�����ش�
			sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
			begin = clock();
		}
	}

	if (recvmsg.checkSYN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
		cout << "�ڶ������ֳɹ�" << endl;
		sendmsg.reset();
		sendmsg.setport(client_port, server_port);
		sendmsg.setACK();
		sendmsg.setSeq(1);
		sendmsg.setAck(1);
		sendmsg.setlen(windows_len);
		memcpy(sendmsg.data, filename.c_str(), filename.length());
		CheckSum(sendmsg, sendheader);
		//���͵���������
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
		cout << "�ڶ�������ʧ��" << endl;
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
		cout << "�ڶ��λ��ֳɹ�" << endl;
	}

	while (recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len)) {
		if (recvmsg.checkFIN() && recvmsg.checkACK() && check(recvmsg, recvheader)) {
			cout << "�����λ��ֳɹ�" << endl;
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
		cout << "���ļ�ʧ��" << endl;
	}
	//��λ���ļ���ͷ
	fin.seekg(0, ios::end);
	filelen = fin.tellg();
	fin.seekg(0, ios::beg);
	file = new char[filelen];
	fin.read(file, filelen);
	fin.close();
}


//�����߳�
//ѭ�����ձ���
/*�����ܱ��ĵ������ڴ����ڵ�ʱ�򣬽����ı��Ϊ�ѽ��ա�
������ܵ��ı�������Ǵ�������С��ţ��򽫴��ڽ��л�������������һ��δȷ�Ͻ��յı��Ĵ���
������ͨ�� while ѭ��������� startid �� �����Ƿ��Ѿ������ա�
����ǣ��������ڻ���һ��λ�ü�����⣬�������˳�ѭ����*/
DWORD WINAPI SR_recvmessage(LPVOID lpThreadParameter) {
	message recvmsg;
	u_long imode = 0;
	ioctlsocket(client_socket, FIONBIO, &imode);
	while (1) {
		//������Ϣ
		recvfrom(client_socket, (char*)&recvmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, &len);
		printmessage1(recvmsg);
		// ����Ƿ���ȷ����Ϣ
		if (recvmsg.checkACK() && check(recvmsg, recvheader)) {
			// ����յ��� ACK ��Ŵ��ڵ��ڴ�����߽磨startid��
			if (startid <= recvmsg.getAck()) {
				recvACK[recvmsg.getAck()] = true; //���Ϊ�ѽ���
				// ���������߽�����ѷ��͵������� + 1�������
				if (startid == maxsent + 1) {
					continue;
				}
				else {
					// ���򣬸��� SR ��ʱ��ʱ��
					SRbegin = clock();
				}
				while (1) {
					// ��鴰���ڵı����Ƿ��Ѿ���ȷ�ϣ�����ǣ����´�����߽�
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
				printf("������߽�=%d, �����ұ߽�=%d\n", startid, (((startid + windows_len) <= alen) ? (startid + windows_len) : alen + 1));
				printf("\n");
				mtx.unlock();
			}
			// ���������߽�����ļ��ܷ����� + 1��˵���Ѿ����������з��飬����ѭ��
			if (startid == alen + 1) {
				break;
			}
		}
	}
	return 1;
}


//�ط��߳�
/*�ط��߳��Ƿ����� resent �������ơ�
�� resent Ϊ true ��ʱ���ط����������з��͵�δȷ�ϵı��ġ�*/
DWORD WINAPI SR_resentmessage(LPVOID lpThreadParameter) {
	message sendmsg;
	while (1) {
		//��������ط�
		if (resent) {
			// ���������ڵ����з���
			for (int i = startid; i <= maxsent; i++) {
				// ��������Ѿ�ȷ�Ͻ��գ�������
				if (recvACK[i]) {
					continue;
				}
				// ����Ҫ�ط�����Ϣ
				sendmsg.reset();
				sendmsg.setport(client_port, server_port);
				if (i == alen) {
					// �ж��Ƿ������һ������
					sendmsg.setEOF(); // ������ϢΪ�ļ�������־
					if (filelen % max_size != 0) {
						// ����ļ����Ȳ��� max_size ��������
						// �����ļ���ʣ�ಿ�ֵ���Ϣ�������ֶ�
						memcpy(sendmsg.data, file + (i - 1) * max_size, filelen % max_size);
					}
					else {
						// ���򣬸��� max_size ���ȵ����ݵ���Ϣ�������ֶ�
						memcpy(sendmsg.data, file + (i - 1) * max_size, max_size); // ������Ϣ�����ݳ���Ϊʣ�ಿ�ֵĳ���
					}
					sendmsg.setlen(filelen % max_size);
				}
				else { // ����������һ������
					// ���� max_size ���ȵ����ݵ���Ϣ�������ֶ�
					memcpy(sendmsg.data, file + (i - 1) * max_size, max_size);
					// ������Ϣ�����ݳ���Ϊ max_size
					sendmsg.setlen(max_size);
				}
				sendmsg.setSeq(i);// ������Ϣ�����Ϊ i
				sendmsg.setAck(i);
				CheckSum(sendmsg, sendheader); // ������Ϣ�����Ϊ i
				// ��ӡ��Ϣ��Ϣ��������ط�״̬������ط���־
				mtx.lock();
				if (resent) {
					printf("���ط���������");
				}
				printmessage(sendmsg);
				printf("\n");
				mtx.unlock();
				// �����ط���Ϣ
				sendto(client_socket, (char*)&sendmsg, sizeof(message), 0, (SOCKADDR*)&server_addr, len);
			}
			// �ط���ɺ� resent ���Ϊ false
			resent = false;
		}
	}
}

//�����ļ�
void SR_sendfiles() {
	cout << "��ʼ����" << endl;
	//����alen������Ŀ��max_sizeΪÿ������Ĵ�С
	alen = int(filelen / max_size);
	if (filelen % max_size != 0) {
		alen += 1;
	}
	//��ʼ������ȷ�����飨recvACK����¼ÿ�������Ƿ��Ѿ����յ�ȷ��
	recvACK = new bool[alen + 1];
	for (int i = 0; i < alen + 1; i++) {
		recvACK[i] = false;
	}

	message sendmsg; //������Ϣ
	startid = 1;
	maxrecv = 0;
	//recvproc ���ڽ���ȷ����Ϣ��resentproc ���ڴ����ط�
	HANDLE recvproc = CreateThread(nullptr, 0, SR_recvmessage, nullptr, 0, nullptr);
	HANDLE resentproc = CreateThread(nullptr, 0, SR_resentmessage, nullptr, 0, nullptr);

	//��¼���俪ʼ��ʱ��
	sendstart1 = clock();

	//���߳�
	//ÿ��ѭ���з��͵�ǰ�����е�δ�����͵����ݡ�
	// �� startid ��ʾ��ǰ���ڵ���߽磬��nextid ��ʾ��ǰ���ڵ��ұ߽磬�� maxsent ��ʾ�ѷ��ͱ��ĵı�š�
	//forѭ������ǰ�����е�����δ���͵ı��Ľ��з��͡�ÿ�η���һ�����ĸ���һ�� maxsent ��
	//�����ʱ���򽫱��� resent ����Ϊ true ���˱���������ط��߳��Ƿ��ط���Ϣ��
	while (1) {
		//���㵱ǰ�����ұ߽�nextid  startid����߽� 
		//���������߽���ϴ��ڴ�СС�ڵ������ݰ��������Ͱ� nextid ����Ϊ��߽���ϴ��ڴ�С����������Ϊ�����ݰ�����
		nextid = ((startid + windows_len) <= alen) ? (startid + windows_len) : alen + 1;
		//maxsent�ѷ��͵����ݰ���� ����maxsent��nextid��Ϣ
		for (int i = maxsent + 1; (i < nextid); i++) {
			//���췢����Ϣ
			sendmsg.reset();//���÷�����Ϣ����
			sendmsg.setport(client_port, server_port);
			if (i == alen) { //���i���������һ�����ݰ���flag��ΪEOF�������ļ����ݵ���Ϣ��
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
			maxsent = i; //����
			if (startid == maxsent) { //������ڵ���߽��������ͱ�ţ���ʾ���ڻ��������� SR ��ʱ��ʱ�� SRbegin
				SRbegin = clock();
			}
		}
		if (startid == alen + 1) { //�ѷ�����ɣ��˳�ѭ��
			break;
		}
		if (clock() - SRbegin > MSL) { //��ʱ resent��ΪTRUE ���ش�
			SRbegin = clock();
			resent = true;
		}
	}
	//��¼�������ʱ��
	sendend1 = clock();
	double ti = (((double)sendend1 - (double)sendstart1) / 1000);
	printf("�ش���ʱ��%d ms\n���ڴ�С:%d\n����ʱ�ӣ�%f s\n�����ʣ�%f byte/s\n", MSL, windows_len, ti, (filelen / ti));
	
}

int main() {
	cout << "��������ļ�" << endl;
	string v;
	cin >> v;
	filename = v;
	cout << "�����봰�ڴ�С" << endl;
	cin >> windows_len;
	client_init();
	connect();
	readfiles();
	SR_sendfiles();
	disconnect();
	return 0;
}