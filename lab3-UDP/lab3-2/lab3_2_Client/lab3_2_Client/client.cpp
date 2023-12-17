#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
using namespace std;

USHORT PORT = 6000;
#define IP "127.0.0.1"

/////////////////////////////////////////////////////////////////////////////
// ===================================��ʽ===================================
/////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
typedef struct Packet_Header
{
    WORD datasize;		// ���ݳ���
    BYTE tag;			// ��ǩ
    //��λ��ʹ�ú���λ��������OVER FIN ACK SYN 
    BYTE window;		// ���ڴ�С
    BYTE seq;			// ���к�
    BYTE ack;			// ȷ�Ϻ�
    WORD checksum;		// У���

    // ��ʼ��
    Packet_Header()
    {
        datasize = 0;
        tag = 0;
        window = 0;
        seq = 0;
        ack = 0;
        checksum = 0;
    }
};
#pragma pack(0)


const BYTE SYN = 0x1;		//SYN = 1 ACK = 0 FIN = 0
const BYTE ACK = 0x2;		//SYN = 0 ACK = 1 FIN = 0
const BYTE ACK_SYN = 0x3;	//SYN = 1 ACK = 1 FIN = 0
const BYTE FIN = 0x4;		//FIN = 1 ACK = 0 SYN = 0
const BYTE FIN_ACK = 0x6;	//FIN = 1 ACK = 1 SYN = 0
const BYTE OVER = 0x8;		//������־
const BYTE END = 0x16;		//ȫ�ֽ�����־


/////////////////////////////////////////////////////////////////////////////////
// ===============================���ຯ��=======================================
/////////////////////////////////////////////////////////////////////////////////

// ����У���
WORD compute_sum(WORD* message, int size) {   // size = 8
    int count = (size + 1) / 2;  // ��ֹ�����ֽ��� ȷ��WORD 16λ
    WORD* buf = (WORD*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, message, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

// �ͻ��������˽������ӣ���ȡ�������ֵķ�ʽ
int Client_Server_Connect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, int label)
{
    Packet_Header packet;
    int size = sizeof(packet);  // size = 8
    char* buffer = new char[sizeof(packet)];  // ����buffer

    // ��һ�Σ��ͻ������������˷��ͽ�����������
    packet.tag = SYN;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto�����������ݳ���" << endl;
        return 0;
    }

    // �趨��ʱ�ش�����
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ������ģʽ
    clock_t start = clock(); //��¼���͵�һ������ʱ��

    // �ڶ��Σ��ͻ��˽��շ���˻ش�������
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //ʱ�䳬��1�� ���Ϊ��ʱ�ش� 
        {
            sendto(socketClient, buffer, size, 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "��һ�����ֳ�ʱ�����ڽ����ش�" << endl;
        }
    }

    cout << "�ɹ����͵�һ��������Ϣ" << endl;

    // �Խ������ݽ���У��ͼ���
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "�ɹ��յ��ڶ���������Ϣ" << endl;
    }
    else {
        cout << "�޷����շ���˻ش������ɿ�����" << endl;
        return 0;
    }

    // �����Σ��ͻ��˷��͵�������������
    packet.tag = ACK_SYN;
    packet.datasize = label;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto�����������ݳ���" << endl;
        return 0;
    }

    cout << "�ɹ����͵�����������Ϣ" << endl;
    cout << "�ͻ��������˳ɹ������������ֽ������ӣ�---------���Կ�ʼ����/��������" << endl;

    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);
    return 1;
}

int Client_Server_Size(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, int Size, int Window, int*& A) {
    Packet_Header packet;
    char* buffer = new char[sizeof(packet)];  // ����buffer

    // ��֪����� �ͻ��˵Ļ�����Ϊ���
    packet.tag = SYN;
    packet.datasize = Size;
    packet.window = Window;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto�����������ݳ���" << endl;
        return -1;
    }

    // �趨��ʱ�ش�����
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ������ģʽ
    clock_t start = clock(); //��¼���͵�һ������ʱ��

    // ���շ���˻ش���֪�Ļ�������С
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //ʱ�䳬��1�� ���Ϊ��ʱ�ش� 
        {
            sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "���ݷ��ͳ�ʱ�����ڽ����ش�" << endl;
        }
    }
    cout << "�ɹ���֪�Է���������С" << endl;

    // �Խ������ݽ���У��ͼ���
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "�ɹ��յ��Է���������С��Ϣ" << endl;
    }
    else {
        cout << "�޷����շ���˻ش������ɿ�����" << endl;
        return -1;
    }

    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ����ģʽ

    A[0] = (Size > packet.datasize) ? packet.datasize : Size;
    A[1] = (Window > packet.window) ? packet.window : Window;
    return 1;
}

// �ͻ�����������˶Ͽ����ӣ���ȡ�Ĵλ��ֵķ�ʽ
int Client_Server_Disconnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
    Packet_Header packet;
    char* buffer = new char[sizeof(packet)];  // ����buffer
    int size = sizeof(packet);

    // �ͻ��˵�һ�η������
    packet.tag = FIN_ACK;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, size);
    memcpy(buffer, &packet, size);
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto�����������ݳ���" << endl;
        return 0;
    }

    clock_t start = clock();   // ��¼��һ�λ��ֵ�ʱ��

    // �ͻ��˽��շ���˷����ĵڶ��λ���
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //ʱ�䳬��1�� ���Ϊ��ʱ�ش� 
        {
            sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "��һ�λ��ֳ�ʱ�����ڽ����ش�" << endl;
        }
    }

    cout << "�ɹ����͵�һ�λ�����Ϣ" << endl;

    // �Խ������ݽ���У��ͼ���
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "�ɹ��յ��ڶ��λ�����Ϣ" << endl;
    }
    else {
        cout << "�ڶ��λ���ʧ��" << endl;
        cout << "�޷����շ���˻ش��Ͽ�����" << endl;
        return 0;
    }

    // �ͻ��˽��շ���˷����ĵ����λ���
    while (1) {
        if (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) == -1) {
            cout << "�����λ���ʧ��" << endl;
            cout << "�޷����շ���˻ش��Ͽ�����" << endl;
            return 0;
            //continue;
        }
        // �Խ������ݽ���У��ͼ���
        memcpy(&packet, buffer, sizeof(packet));
        if ((packet.tag == FIN_ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "�ɹ��յ������λ�����Ϣ" << endl;
            break;
        }
    }

    // ���ĴΣ��ͻ��˷��͵��Ĵλ�������
    packet.tag = ACK;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto�����������ݳ���" << endl;
        return 0;
    }

    cout << "�ɹ����͵��Ĵλ�����Ϣ" << endl;
    cout << "�ͻ��������˳ɹ��Ͽ����ӣ�" << endl;

    return 1;
}

// ��Ϊ���ͷ���������
void Send_Message(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* Message, int mes_size, int MAX_SIZE, int Window) {
    int packet_num = mes_size / (MAX_SIZE)+(mes_size % MAX_SIZE != 0);
    int Seq_num = 0;  //��ʼ�����к�
    int Ack_num = 1;
    Packet_Header packet;
    int confirmed = -1;
    int cur = 0;
    int last = 0; //��ǰδ�յ�ȷ�ϵ����ݱ����
    char** Staging = new char* [Window];   //�����ݴ���
    int* Staging_len = new int[Window];
    for (int i = 0; i < Window; i++) {
        Staging[i] = new char[sizeof(packet) + MAX_SIZE];
    }
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ������ģʽ
    while (confirmed < (packet_num - 1)) {  // ���з������ݾ���ȷ��
        clock_t* time = new clock_t[Window];
        while (cur <= confirmed + Window && cur < packet_num) {
            int data_len = (cur == packet_num - 1 ? mes_size - (packet_num - 1) * MAX_SIZE : MAX_SIZE);
            //char* buffer = new char[sizeof(packet) + data_len];  // ����buffer
            packet.tag = 0;
            packet.seq = Seq_num++;
            Seq_num = (Seq_num > 255 ? Seq_num - 256 : Seq_num);
            packet.datasize = data_len;
            Staging_len[cur % Window] = data_len;
            packet.window = Window - (cur - confirmed);  //ʣ�໬�����ڴ�С
            packet.checksum = 0;
            packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
            memcpy(Staging[cur % Window], &packet, sizeof(packet));
            char* mes = Message + cur * MAX_SIZE;
            memcpy(Staging[cur % Window] + sizeof(packet), mes, data_len);

            // ��������
            sendto(socketClient, Staging[cur % Window], sizeof(packet) + data_len, 0, (sockaddr*)&servAddr, servAddrlen);
            // ��¼ÿһ�εķ���ʱ��
            time[cur % Window] = clock();
            cout << "Begin sending message...... datasize:" << data_len << " bytes!"
                << " seq:" << int(packet.seq) << " window:" << int(packet.window)
                << " checksum:" << int(packet.checksum) << "ʱ��洢��ţ�" << last << endl;
            cur++;
        }

        char* re_buffer = new char[sizeof(packet)];
        while (recvfrom(socketClient, re_buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
            int time_temp = last;
            if ((clock() - time[last]) / 1000 > 2)   //ʱ�䳬��2�� ���Ϊ��ʱ�ش� 
            {  //�ش�����δ��ack������
                cout << "====================" << last << endl;
                for (int temp = confirmed + 1; temp <= confirmed + Window && temp < packet_num; temp++) {
                    // ��ʱ���·�������
                    sendto(socketClient, Staging[temp % Window], sizeof(packet) + Staging_len[cur % Window], 0, (sockaddr*)&servAddr, servAddrlen);
                    cout << "��ʱ���·����ݴ�������������:  ���к�Ϊ" << temp % 256 << "ʱ��洢��ţ�" << time_temp << endl;
                    time[time_temp] = clock();
                    time_temp = (time_temp + 1) % Window;
                }
            }
        }

        //�ɹ��յ���Ӧ
        memcpy(&packet, re_buffer, sizeof(packet));
        if ((compute_sum((WORD*)&packet, sizeof(packet)) != 0)) {  //����У�����
            continue;
        }
        //�յ���ȷ��Ӧ ��������
        if (packet.ack == Ack_num) {
            Ack_num = (Ack_num + 1) % 256;
            cout << "Successful send and recv response  ack:" << int(packet.ack) << "ʱ��洢��ţ�" << last << endl;
            confirmed++;
            time[last] = clock();
            last = (last + 1) % Window;
        }
        else {
            //���յ���ȷ�Ϻ���������ȷ�Ϻ�֮��Ĳ�ֵ
            int dis = (int(packet.ack) - Ack_num) > 0 ? (int(packet.ack) - Ack_num) : (int(packet.ack) + 256 - Ack_num);
            //�����ظ�ACK
            if (packet.ack == (Ack_num + 256 - 1) % 256) {
                cout << "recv (same ACK)!!!!" << int(packet.ack) << endl;
                continue;
            }
            else if (dis < Window) {
                // �ۼ�ȷ��
                for (int temp = 0; temp <= dis; temp++) {
                    cout << "===============�ۼ�ȷ��===============" << endl;
                    cout << "Successful send and recv response  ack:" << Ack_num + temp << endl;
                    confirmed++;
                    time[last] = clock();
                    last = (last + 1) % Window;
                }
                Ack_num = (int(packet.ack) + 1) % 256;  //�����´��ڴ���ACK���
            }
            else {  //����У�����ack������  �ط�����
                for (int temp = confirmed + 1; temp <= confirmed + Window && temp < packet_num; temp++) {
                    // ��ʱ���·�������
                    sendto(socketClient, Staging[temp % Window], sizeof(packet) + Staging_len[cur % Window], 0, (sockaddr*)&servAddr, servAddrlen);
                    cout << "�������·����ݴ�������������:  ���к�Ϊ" << temp % 256 << endl;
                }
            }
        }
    }
    //���ͽ�����־
    packet.tag = OVER;
    char* buffer = new char[sizeof(packet)];
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "The end tag has been sent" << endl;
    clock_t start = clock();
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //ʱ�䳬��1�� ���Ϊ��ʱ�ش� 
        {
            sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
            cout << "���ݷ��ͳ�ʱ�������ش�������" << endl;
            start = clock();
        }
    }
    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ����ģʽ
    memcpy(&packet, buffer, sizeof(packet));
    if (packet.tag == OVER && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "The end token was successfully received" << endl;
    }
    else {
        cout << "The end token wasn't successfully received" << endl;
    }
    return;
}

// ��Ϊ���շ���������
int Recv_Message(SOCKET& socketServer, SOCKADDR_IN& clieAddr, int& clieAddrlen, char* Mes, int MAX_SIZE, int Window) {
    Packet_Header packet;
    char* buffer = new char[sizeof(packet) + MAX_SIZE];  // ����buffer
    int ack = 1;  // ȷ�����к�
    int seq = 0;
    long F_Len = 0;     //�����ܳ�
    int S_Len = 0;      //�������ݳ���
    bool test_state = true;
    //��������
    while (1) {
        while (recvfrom(socketServer, buffer, sizeof(packet) + MAX_SIZE, 0, (sockaddr*)&clieAddr, &clieAddrlen) <= 0);
        memcpy(&packet, buffer, sizeof(packet));

        if (((rand() % (255 - 1)) + 1) == 199 && test_state) {
            cout << int(packet.seq) << "�����ʱ�ش�����" << endl;
            test_state = false;
            continue;
        }

        // END ȫ�ֽ���
        if (packet.tag == END && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "The all_end tag has been recved" << endl;
            return 999;
        }

        // �������
        if (packet.tag == OVER && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "The end tag has been recved" << endl;
            break;
        }

        // ��������
        if (packet.tag == 0 && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            // �Խ������ݼ���ȷ�ϣ����seqǰ������δ���յ� �������ݰ� �ش���ǰ�����ACK
            if (packet.seq != seq) {   // seq�ǵ�ǰ������յ������ŷ���
                Packet_Header temp;
                temp.tag = 0;
                temp.ack = ack - 1;  //�ش�ACK
                temp.checksum = 0;
                temp.checksum = compute_sum((WORD*)&temp, sizeof(temp));
                memcpy(buffer, &temp, sizeof(temp));
                sendto(socketServer, buffer, sizeof(temp), 0, (sockaddr*)&clieAddr, clieAddrlen);
                cout << "Send a resend flag (same ACK) to the client" << endl;
                continue;
            }

            // �ɹ��յ�����
            S_Len = packet.datasize;
            cout << "Begin recving message...... datasize:" << S_Len << " bytes!" << " flag:"
                << int(packet.tag) << " seq:" << int(packet.seq) << " checksum:" << int(packet.checksum) << endl;
            memcpy(Mes + F_Len, buffer + sizeof(packet), S_Len);
            F_Len += S_Len;

            // ���� ��֪�ͻ����ѳɹ��յ�
            packet.tag = 0;
            packet.ack = ack++;
            packet.seq = seq++;
            packet.datasize = 0;
            packet.checksum = 0;
            packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
            memcpy(buffer, &packet, sizeof(packet));
            if (((rand() % (255 - 1)) + 1) != 187) {
                sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen);
            }
            else
                cout << "���кţ�" << int(packet.seq) << " ========�ۼ�ȷ�ϲ���========" << endl;
            cout << "Successful recv and send response  ack : " << int(packet.ack) << endl;
            seq = (seq > 255 ? seq - 256 : seq);
            ack = (ack > 255 ? ack - 256 : ack);
        }
        else if (packet.tag == 0) {  // У��ͼ��㲻��ȷ�����ݳ����֪���ش�
            Packet_Header temp;
            temp.tag = 0;
            temp.ack = ack - 1;  //���ս������������ �ش�ACK
            temp.checksum = 0;
            temp.checksum = compute_sum((WORD*)&temp, sizeof(temp));
            memcpy(buffer, &temp, sizeof(temp));
            sendto(socketServer, buffer, sizeof(temp), 0, (sockaddr*)&clieAddr, clieAddrlen);
            cout << "Send a resend flag (errro ACK) to the client" << endl;
            continue;
        }
    }

    // ���ͽ�����־
    packet.tag = OVER;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen);
    cout << "The end tag has been sent" << endl;
    return F_Len;
}



////////////////////////////////////////////////////////////////////////////////////////
// ====================================main============================================
// ////////////////////////////////////////////////////////////////////////////////////
// ���ܰ������������ӡ�����⡢ȷ���ش��ȡ�
// ���ڻ������ڵ��������ƻ���

int main()
{
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        cout << "Startup ����" << endl;
        WSACleanup();
        return 0;
    }
    SOCKET client = socket(AF_INET, SOCK_DGRAM, 0);
    if (client == -1)
    {
        cout << "socket ���ɳ���" << endl;
        WSACleanup();
        return 0;
    }
    cout << "�ͻ����׽��ִ����ɹ���" << endl;

    // �����Ϣ
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &serveraddr.sin_addr.s_addr);

    // ����ѡ��
    cout << "������ѡ���֪����������0 / ����1�����ݣ�";
    int label;
    cin >> label;

    // ��������
    int length = sizeof(serveraddr);
    cout << "��ʼ�����˽�����������......" << endl;
    Client_Server_Connect(client, serveraddr, length, label);

    // ���뻺������С
    cout << "�����뱾�˵����������ݻ�������С��";
    int SIZE;
    cin >> SIZE;
    cout << "�����뱾�˻������ڻ�������С��";
    int Window;
    cin >> Window;
    int* S_W = new int[2];
    SIZE = Client_Server_Size(client, serveraddr, length, SIZE, Window, S_W);
    SIZE = S_W[0];
    Window = S_W[1];
    cout << "˫��Э���ĵ������ݷ��ʹ�СΪ��" << SIZE << "���������ڴ�СΪ��" << Window << endl;


    if (label == 0) {
        while (1) {
            cout << "=====================================================================" << endl;
            cout << "Waiting to choose File!!!" << endl;
            //�����ļ���
            char InFileName[20];
            //�ļ����ݳ���
            int F_length;
            //����Ҫ��ȡ��ͼ����
            cout << "Enter File name:";
            cin >> InFileName;

            // ȫ�ֽ���
            if (InFileName[0] == 'q' && strlen(InFileName) == 1) {
                Packet_Header packet;
                char* buffer = new char[sizeof(packet)];  // ����buffer
                packet.tag = END;
                packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
                memcpy(buffer, &packet, sizeof(packet));
                sendto(client, buffer, sizeof(packet), 0, (sockaddr*)&serveraddr, length);
                cout << "Send a all_end flag to the server" << endl;
                break;
            }

            //�ļ�ָ��
            FILE* fp;
            //�Զ����Ʒ�ʽ��ͼ��
            if ((fp = fopen(InFileName, "rb")) == NULL)
            {
                cout << "Open file failed!" << endl;
                exit(0);
            }
            //��ȡ�ļ������ܳ���
            fseek(fp, 0, SEEK_END);
            F_length = ftell(fp);
            rewind(fp);
            //�����ļ����ݳ��ȷ����ڴ�buffer
            char* FileBuffer = (char*)malloc(F_length * sizeof(char));
            //��ͼ�����ݶ���buffer
            fread(FileBuffer, F_length, 1, fp);
            fclose(fp);

            cout << "�����ļ����ݴ�С��" << F_length << "bytes!" << endl;

            Send_Message(client, serveraddr, length, InFileName, strlen(InFileName), SIZE, Window);
            clock_t start = clock();
            Send_Message(client, serveraddr, length, FileBuffer, F_length, SIZE, Window);
            clock_t end = clock();
            cout << "������ʱ��Ϊ:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
            cout << "������Ϊ:" << ((float)F_length) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;
            cout << "=====================================================================" << endl;
        }
    }
    else {
        while (1) {
            cout << "=====================================================================" << endl;
            cout << "Waiting to receive!!!" << endl;
            char* F_name = new char[20];
            char* Message = new char[100000000];
            int name_len = Recv_Message(client, serveraddr, length, F_name, SIZE, Window);

            // ȫ�ֽ���
            if (name_len == 999)
                break;

            int file_len = Recv_Message(client, serveraddr, length, Message, SIZE, Window);
            string a;
            for (int i = 0; i < name_len; i++) {
                a = a + F_name[i];
            }
            cout << "�����ļ�����" << a << endl;
            cout << "�����ļ����ݴ�С��" << file_len << "bytes!" << endl;

            FILE* fp;
            if ((fp = fopen(a.c_str(), "wb")) == NULL)
            {
                cout << "Open File failed!" << endl;
                exit(0);
            }
            //��buffer��д���ݵ�fpָ����ļ���
            fwrite(Message, file_len, 1, fp);
            cout << "Successfully receive the data in its entirety and save it" << endl;
            cout << "=====================================================================" << endl;
            //�ر��ļ�ָ��
            fclose(fp);
        }
    }
    Client_Server_Disconnect(client, serveraddr, length);
    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}