#include <sys/types.h>
#include <string.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/types.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

USHORT PORT = 7000;
#define IP "127.0.0.1"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==============================================��ʽ===============================================
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
typedef struct Packet_Header
{
    WORD datasize;		// ���ݳ���
    BYTE tag;			// ��ǩ
                        //��λ��ʹ�ú���λ��������OVER FIN ACK SYN 
    BYTE window;		// ���ڴ�С unused
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==================================================���ຯ��=========================================================
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

// �ͻ��������˽������ӣ���ȡ�������ֵķ�ʽ��
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
    ioctlsocket(socketClient, FIONBIO, &mode);//��Ϊ����ģʽ
    return 1;
}

int Client_Server_Size(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, int Size) {
    Packet_Header packet;
    int size = sizeof(packet);  // size = 8
    char* buffer = new char[sizeof(packet)];  // ����buffer

    // ��֪����� �ͻ��˵Ļ�����Ϊ���
    packet.tag = SYN;
    packet.datasize = Size;
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

    // ���շ���˻ش���֪�Ļ�������С
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //ʱ�䳬��1�� ���Ϊ��ʱ�ش� 
        {
            sendto(socketClient, buffer, size, 0, (sockaddr*)&servAddr, servAddrlen);
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
        return 0;
    }

    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ����ģʽ

    if (Size > packet.datasize)
        return packet.datasize;
    else
        return Size;
}

// �ͻ�����������˶Ͽ����ӣ���ȡ�Ĵλ��ֵķ�ʽ��
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
            cout << "��һ�����ֳ�ʱ�����ڽ����ش�" << endl;
        }
    }

    cout << "�ɹ����͵�һ�λ�����Ϣ" << endl;

    // �Խ������ݽ���У��ͼ���
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "�ɹ��յ��ڶ��λ�����Ϣ" << endl;
    }
    else {
        cout << "�޷����շ���˻ش��Ͽ�����" << endl;
        return 0;
    }

    // �ͻ��˽��շ���˷����ĵ����λ���
    while (1) {
        if (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) == -1) {
            cout << "�޷����շ���˻ش��Ͽ�����" << endl;
            return 0;
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

// �������ݰ�

void Send_Message(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* Message, int mes_size, int MAX_SIZE)
{
    int packet_num = mes_size / (MAX_SIZE)+(mes_size % MAX_SIZE != 0); //������Ҫ�ֳɶ��ٸ����ݰ�������������Ϣ��
    int Seq_num = 0;  //��ʼ�����кţ����ڱ�ʶ���ݰ���˳��
    Packet_Header packet;//�洢���ݰ���ͷ����Ϣ
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ������ģʽ

    for (int i = 0; i < packet_num; i++)
    {
        // ���㵱ǰ���ݰ��ĳ���
        int data_len = (i == packet_num - 1 ? mes_size - (packet_num - 1) * MAX_SIZE : MAX_SIZE);
        // ���䷢�ͻ�����
        char* buffer = new char[sizeof(packet) + data_len];  // ����buffer
         // �������ݰ���ͷ����Ϣ
        packet.tag = 0;
        packet.seq = Seq_num;
        packet.datasize = data_len;
        packet.checksum = 0;
        packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
        memcpy(buffer, &packet, sizeof(packet));
        // ����Ϣ���ݸ��Ƶ�������
        char* mes = Message + i * MAX_SIZE;
        memcpy(buffer + sizeof(packet), mes, data_len);

        // ��������
        sendto(socketClient, buffer, sizeof(packet) + data_len, 0, (sockaddr*)&servAddr, servAddrlen);
        cout << "Begin sending message...... datasize:" << data_len << " bytes!" << " flag:"
            << int(packet.tag) << " seq:" << int(packet.seq) << " checksum:" << int(packet.checksum) << endl;

        clock_t start = clock();    //��¼����ʱ��

        while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
            if ((clock() - start) / 1000 > 1)   //ʱ�䳬��1�� ���Ϊ��ʱ�ش� 
            {
                sendto(socketClient, buffer, sizeof(packet) + data_len, 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "���ݷ��ͳ�ʱ�������ش�������" << endl;
                start = clock();
            }
        }

        memcpy(&packet, buffer, sizeof(packet));
        if (packet.ack == (Seq_num + 1) % (256) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "Successful send and recv response  ack:" << int(packet.ack) << endl;
        }
        else {
            // �����δ���ܵ����� �ش�����
            // ����˽��յ������� У��ͳ��� ��Ҫ�ش�
            if (packet.ack == Seq_num || (compute_sum((WORD*)&packet, sizeof(packet)) != 0)) {
                cout << "�����δ���ܵ����ݣ������ش�������" << endl;
                i--;
                continue;
            }
            else {
                cout << "�޷����շ���˻ش�" << endl;
                return;
            }
        }
        //�������к�
        // Seq_num�� 0-255֮��
        Seq_num++;
        Seq_num = (Seq_num > 255 ? Seq_num - 256 : Seq_num);
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

    memcpy(&packet, buffer, sizeof(packet));
    if (packet.tag == OVER && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "The end token was successfully received" << endl;
    }
    else {
        cout << "�޷����շ���˻ش�" << endl;
        return;
    }
    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // ����ģʽ
    return;
}


// �ͻ��˽�������
int Recv_Messsage(SOCKET& socketServer, SOCKADDR_IN& clieAddr, int& clieAddrlen, char* Mes, int MAX_SIZE)
{
    Packet_Header packet;
    char* buffer = new char[sizeof(packet) + MAX_SIZE];  // ����buffer
    int ack = 1;  // ȷ�����к�
    int seq = 0;
    long F_Len = 0;     //�����ܳ�
    int S_Len = 0;      //�������ݳ���

    // �����ļ�
    while (1) {
        while (recvfrom(socketServer, buffer, sizeof(packet) + MAX_SIZE, 0, (sockaddr*)&clieAddr, &clieAddrlen) <= 0);
        memcpy(&packet, buffer, sizeof(packet));

        if (((rand() % (255 - 1)) + 1) == 199) {
            cout << int(packet.seq) << "�����ʱ�ش�����" << endl;
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

            // �Խ������ݼ���ȷ�ϣ������ͱ�Ǹ�֪�ͻ�����Ҫ�ط�
            if (packet.seq != seq) {
                Packet_Header temp;
                temp.tag = 0;
                temp.ack = seq;
                temp.checksum = 0;
                temp.checksum = compute_sum((WORD*)&temp, sizeof(temp));
                memcpy(buffer, &temp, sizeof(temp));
                sendto(socketServer, buffer, sizeof(temp), 0, (sockaddr*)&clieAddr, clieAddrlen);
                cout << "Send a resend flag to the client" << endl;
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
            sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen);
            cout << "Successful recv and send response  ack : " << int(packet.ack) << endl;
            seq = (seq > 255 ? seq - 256 : seq);
            ack = (ack > 255 ? ack - 256 : ack);
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//=========================================== main =====================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





// ���ܰ������������ӡ�����⡢ȷ���ش��ȡ�
// �������Ʋ���ͣ�Ȼ���

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
    cout << "��ѡ�񡪡�������0 / ����1�����ݣ�";
    int label;
    cin >> label;

    // ��������
    int length = sizeof(serveraddr);
    cout << "��ʼ�����˽�����������......" << endl;
    Client_Server_Connect(client, serveraddr, length, label);

    // ���뻺������С
    cout << "�����뱾�����ݻ�������С��";
    int SIZE;
    cin >> SIZE;
    SIZE = Client_Server_Size(client, serveraddr, length, SIZE);

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

            Send_Message(client, serveraddr, length, InFileName, strlen(InFileName), SIZE);
            clock_t start = clock();
            Send_Message(client, serveraddr, length, FileBuffer, F_length, SIZE);
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
            int name_len = Recv_Messsage(client, serveraddr, length, F_name, SIZE);

            // ȫ�ֽ���
            if (name_len == 999)
                break;

            int file_len = Recv_Messsage(client, serveraddr, length, Message, SIZE);
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