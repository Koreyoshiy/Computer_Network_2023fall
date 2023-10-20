#include <WinSock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include <ctime>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

#define SEND_OVER 1							 //��Ϣ��ת��
#define SEND_YET  0							 //��Ϣδת��
int i = 0;//�������߿ͻ��˵�����
int Status = SEND_YET;//��Ϣ����״̬����ʼΪδת��

SOCKET ServerSocket = INVALID_SOCKET;		 //������׽��֣����ܿͻ��˵�����
                                             //=INVALID_SOCKET ��ʾ�������׽�����δ����Ч�س�ʼ����
SOCKADDR_IN ClientAddr = { 0 };			 //�ͻ��˵�ַ
int ClientAddrLen = sizeof(ClientAddr);//�ͻ��˵�ַ����

bool CheckConnect = false;                //����������
HANDLE HandleRecv[10] = { NULL };				 //���飬�洢�߳̾����ÿ�������Ӧһ���ͻ��˵Ľ����߳�
HANDLE mainHandle;							 //����accept���߳̾�������ڴ�����ܿͻ�������������̣߳�

//�ͻ�����Ϣ�ṹ��
typedef struct _Client
{
	SOCKET sClient;      //�ͻ����׽���
	char buffer[128];		 //���ݻ�����
	char userName[16];   //�ͻ����û���
	char identify[16];   //���ڱ�ʶת���ķ�Χ
	char IP[20];		 //�ͻ���IP
	UINT_PTR flag;       //��ǿͻ��ˣ��������ֲ�ͬ�Ŀͻ���
} Client;

Client insideClient[10] = { 0 };                  //����һ���ͻ��˽ṹ��,���ͬʱ10������

//���������߳�
unsigned __stdcall ThreadSend(void* param)
{
	int error = 0;
	int flag = *(int*)param;//�����ݸ��̵߳Ĳ��� param ת��Ϊ�����������丳ֵ�� flag��flag ��ʾҪ������Ϣ�Ŀͻ��˵ı�־
	SOCKET client = INVALID_SOCKET;					//����һ����ʱ�׽��������Ҫת���Ŀͻ����׽���
	char temp[128] = { 0 };							//����һ����ʱ�����ݻ�������������Ž��յ�������
	memcpy(temp, insideClient[flag].buffer, sizeof(temp));//�� insideClient[flag].buffer �е���Ϣ���Ƶ� temp �У��Ա������޸ĺͷ��͡�
	sprintf_s(insideClient[flag].buffer, "%s: %s", insideClient[flag].userName, temp); //�ѷ���Դ���������ת������Ϣ��
	if (strlen(temp) != 0 && Status == SEND_YET) //������ݲ�Ϊ���һ�ûת����ת��
	{
		if (strcmp(insideClient[flag].identify, "all") == 0)//�����all��ת��������������
		{
			for (int j = 0; j < i; j++) {
				if (j != flag) {//����Լ�֮������пͻ��˷�����Ϣ
					error = send(insideClient[j].sClient, insideClient[flag].buffer, sizeof(insideClient[j].buffer), 0);
					if (error == SOCKET_ERROR)
						return 1;
				}
			}
		}
		else//���򷢸�ָ�����Ǹ���
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

	Status = SEND_OVER;   //ת���ɹ�������״̬Ϊ��ת��
	return 0;
}

//���������߳�
unsigned __stdcall ThreadRecv(void* param)
{
	//// ��ȡ��ǰʱ��
	//time_t now = time(0);
	//// ת��Ϊ����ʱ��
	//tm* ltm = localtime(&now);


	SOCKET client = INVALID_SOCKET;
	int flag = 0;//���Ҫ����Ŀͻ��˵�����
	for (int j = 0; j < i; j++) {
		if (*(int*)param == insideClient[j].flag)            //�ж���Ϊ�ĸ��ͻ��˿��ٵĽ��������߳�
		{
			client = insideClient[j].sClient;
			flag = j;
		}
	}
	char temp[128] = { 0 };  //��ʱ���ݻ�����
	while (true)//��ѭ�����ڲ��Ͻ������Կͻ��˵���Ϣ
	{
		// ��ȡ��ǰʱ��
		time_t now = time(0);
		// ת��Ϊ����ʱ��
		tm* ltm = localtime(&now);

		memset(temp, 0, sizeof(temp));//�� temp ���������ȫ����ʼ��Ϊ�㣬��ȷ��������Ϊ�ա�
		int error = recv(client, temp, sizeof(temp), 0); //��������
		if (error == SOCKET_ERROR)
			continue;
		Status = SEND_YET;	//����ת��״̬Ϊδת��
		memcpy(insideClient[flag].buffer, temp, sizeof(insideClient[flag].buffer));/*�����յ�����Ϣ�� temp ���������Ƶ�
																				   ��Ӧ�ͻ��˵���Ϣ������ insideClient[flag].buffer ��*/
		if (strcmp(temp, "exit") == 0)   //�ж�����ͻ�����exit������ôֱ�ӹر��̣߳�����ת���߳�
		{
			closesocket(insideClient[flag].sClient);//�رո��׽���
			CloseHandle(HandleRecv[flag]); //����ر����߳̾��
			insideClient[flag].sClient = 0;  //�����λ�ÿճ����������Ժ������߳�ʹ��
			HandleRecv[flag] = NULL;
			//��printf��ȡ����
			cout << ltm->tm_hour << ":" << ltm->tm_min<<  "��" <<insideClient[flag].IP << "��" << "'" << insideClient[flag].userName<< "'" << "has disconnected from the server. " << endl;
		}
		else
		{
			cout << ltm->tm_hour << ":" << ltm->tm_min << "��" << insideClient[flag].IP << "��" << insideClient[flag].userName << ": " << temp << endl;
			_beginthreadex(NULL, 0, ThreadSend, &flag, 0, NULL); //����һ��ת���߳�,flag����������Ҫ��ת������Ϣ�Ǵ��ĸ��߳����ģ�
		}
	}
	return 0;
}


//��������
unsigned __stdcall MainAccept(void* param)
{
	//// ��ȡ��ǰʱ��
	//time_t now = time(0);
	//// ת��Ϊ����ʱ��
	//tm* ltm = localtime(&now);

	int flag[10] = { 0 };
	while (true)//�����ͽ��տͻ��˵���������
	{
		if (insideClient[i].flag != 0)   //�ҵ���ǰ�����һ��û�����ӵ�insideClient
		{
			i++;
			continue;
		}
		//����пͻ����������Ӿͽ�������
		if ((insideClient[i].sClient = accept(ServerSocket, (SOCKADDR*)&ClientAddr, &ClientAddrLen)) == INVALID_SOCKET)
		{
			cout << "Accept error: " << WSAGetLastError() << endl;
			closesocket(ServerSocket);
			WSACleanup();
			return -1;
		}
		else if (i > 10) {
			cout << "����ʧ�ܣ������ѳ����ޣ�" << endl;
			closesocket(ServerSocket);
			WSACleanup();
			return -1;
		}
		recv(insideClient[i].sClient, insideClient[i].userName, sizeof(insideClient[i].userName), 0); //�����û���
		recv(insideClient[i].sClient, insideClient[i].identify, sizeof(insideClient[i].identify), 0); //����ת���ķ�Χ
		
		memcpy(insideClient[i].IP, inet_ntoa(ClientAddr.sin_addr), sizeof(insideClient[i].IP)); //��¼�ͻ���IP

		// ��ȡ��ǰʱ��
		time_t now = time(0);
		// ת��Ϊ����ʱ��
		tm* ltm = localtime(&now);

		cout << ltm->tm_hour << ":" << ltm->tm_min<< "The user: '" << insideClient[i].userName << "'" << "��" << insideClient[i].IP << "��"  << " connect successfully!"  << endl;
		//memcpy(insideClient[i].IP, inet_ntoa(ClientAddr.sin_addr), sizeof(insideClient[i].IP)); //��¼�ͻ���IP
		insideClient[i].flag = insideClient[i].sClient; //��ͬ��socke�в�ͬUINT_PTR���͵���������ʶ
		i++;    //���ȥ������ʼ�ĸ�ֵ�ᱨ��
		for (int j = 0; j < i; j++)
		{
			if (insideClient[j].flag != flag[j])
			{
				if (HandleRecv[j])   //����Ǹ�����Ѿ��������ˣ���ô�Ǿ͹ص��Ǹ����
					CloseHandle(HandleRecv[j]);
				HandleRecv[j] = (HANDLE)_beginthreadex(NULL, 0, ThreadRecv, &insideClient[j].flag, 0, NULL); //����������Ϣ���߳�
			}
		}

		for (int j = 0; j < i; j++)
		{
			flag[j] = insideClient[j].flag;//��ֹThreadRecv�̶߳�ο���
		}
		Sleep(3000);
	}
	return 0;
}

int main()
{
	WSADATA wsaData = { 0 };

	//��ʼ���׽���
	//ָ��Ҫʹ�õ� Winsock �汾2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "WSAStartup error: " << WSAGetLastError() << endl;
		return -1;
	}

	//�����׽���
	ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ServerSocket == INVALID_SOCKET)
	{
		cout << "Socket error: " << WSAGetLastError() << endl;
		return -1;
	}

	SOCKADDR_IN ServerAddr = { 0 };				//����˵�ַ
	USHORT uPort = 1666;						//�����������˿�
	//���÷�������ַ
	ServerAddr.sin_family = AF_INET;//���ӷ�ʽ
	ServerAddr.sin_port = htons(uPort);//�����������˿�
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//�κοͻ��˶����������������

	//�󶨷�����
	if (SOCKET_ERROR == bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "Bind error: " << WSAGetLastError() << endl;
		closesocket(ServerSocket);
		return -1;
	}

	//���ü����ͻ���������
	if (SOCKET_ERROR == listen(ServerSocket, 20))
	{
		cout << "Listen error: " << WSAGetLastError() << endl;
		closesocket(ServerSocket);
		WSACleanup();
		return -1;
	}

	cout << "�ȴ��û�����..." << endl;
	mainHandle = (HANDLE)_beginthreadex(NULL, 0, MainAccept, NULL, 0, 0);   //mainHandle����������client����Ҫ�߳�
	char Serversignal[10];
	cout << "�رշ�����������'exit' " << endl;
	while (true)
	{
		cin.getline(Serversignal, 10);
		if (strcmp(Serversignal, "exit") == 0)
		{
			cout << "�������ѹر�. " << endl;
			CloseHandle(mainHandle);
			for (int j = 0; j < i; j++) //���ιر��׽���
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