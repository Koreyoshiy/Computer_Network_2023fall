#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

#define PORT 7001
#define IP "127.0.0.2"

/////////////////////////////////////////////////////////////////////////////////////////
// ===================================格式==========================================
/////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
struct Packet_Header
{
    WORD datasize;		// 数据长度
    BYTE tag;			// 标签
                        //八位，使用后四位，排列是OVER FIN ACK SYN 
    BYTE window;		// 窗口大小(本次没用到)
    BYTE seq;			// 序列号
    BYTE ack;			// 确认号
    WORD checksum;		// 校验和

    // 初始化
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
const BYTE OVER = 0x8;		//结束标志
const BYTE END = 0x16;		//全局结束标志



/////////////////////////////////////////////////////////////////////////////////////////
// ====================================各类函数=========================================
/////////////////////////////////////////////////////////////////////////////////////////



// 计算校验和
WORD compute_sum(WORD* message, int size) {   // size = 8
    int count = (size + 1) / 2;  // 防止奇数字节数 确保WORD 16位
    WORD* buf = (WORD*)malloc(size + 1);// 创建一个缓冲区用于存储数据
    memset(buf, 0, size + 1);// 将缓冲区初始化为0
    memcpy(buf, message, size);// 将数据复制到缓冲区中
    u_long sum = 0;// 用于存储校验和的变量
    while (count--) {
        sum += *buf++;// 逐个字节累加到校验和中
        if (sum & 0xffff0000) { // 如果校验和溢出了16位，则进行溢出处理
            sum &= 0xffff; // 将校验和截断为16位
            sum++;// 溢出处理，将校验和加1
        }
    }
    return ~(sum & 0xffff);// 返回校验和的反码
}

// 客户端与服务端建立连接（采取三次握手的方式）
int Client_Server_Connect(SOCKET& socketServer, SOCKADDR_IN& clieAddr, int& clieAddrlen)
{
    Packet_Header packet;// 定义握手信息的数据结构
    char* buffer = new char[sizeof(packet)];  // 发送buffer

    // 第一次握手：服务端接收客户端发送的连接请求
    while (1) {
        if (recvfrom(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, &clieAddrlen) == -1) {
            cout << "无法接收客户端发送的连接请求" << endl;
            return -1;
        }
        memcpy(&packet, buffer, sizeof(packet));// 将接收到的数据复制到packet结构体中
        if (packet.tag == SYN && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "成功接收第一次握手信息" << endl;
            break;
        }
    }

    // 第二次握手：服务端向客户端发送握手信息
    packet.tag = ACK;// 设置握手信息的tag字段为ACK
    packet.checksum = 0;// 初始化校验和字段为0
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));// 计算校验和
    memcpy(buffer, &packet, sizeof(packet));// 将握手信息复制到buffer中
    if (sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen) == -1)
    {
        return -1;
    }
    u_long mode = 1;
    ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为非阻塞模式
    clock_t start = clock();//记录第二次握手发送时间

    // 第三次握手：服务端接收客户端发送的握手信息
    while (recvfrom(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, &clieAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
        {
            sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen);
            start = clock();
            cout << "第二次握手超时，正在进行重传" << endl;
        }
    }
    mode = 0;
    ioctlsocket(socketServer, FIONBIO, &mode);  // 设置为阻塞模式

    cout << "成功发送第二次握手信息" << endl;

    // 对接收数据进行校验和检验
    memcpy(&packet, buffer, sizeof(packet));// 将接收到的数据复制回packet结构
    if ((packet.tag == ACK_SYN) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "成功收到第三次握手信息" << endl;
    }
    else {
        cout << "无法接收客户端回传建立可靠连接" << endl;
        return -1;
    }

    cout << "客户端与服务端成功进行三次握手建立连接！---------可以开始发送/接收数据" << endl;

    return int(packet.datasize);// 返回数据的大小
}




int Client_Server_Size(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, int size) {
    Packet_Header packet;
    char* buffer = new char[sizeof(packet)];  // 发送buffer

    // 接收客户端告知的缓冲区数据大小
    while (1) {
        if (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) == -1) {
            cout << "无法接收客户端发送的连接请求" << endl;
            return -1;
        }
        memcpy(&packet, buffer, sizeof(packet));
        if (packet.tag == SYN && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "成功收到对方缓冲区大小信息" << endl;
            break;
        }
    }
    int client_size = packet.datasize;

    // 服务端告知客户端缓冲区大小信息
    packet.tag = ACK;
    packet.datasize = size;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        return -1;
    }
    cout << "成功告知对方缓冲区大小" << endl;

    if (size > client_size)
        return client_size;
    else
        return size;
}

// 客户端与服务器端断开连接（采取四次挥手的方式）
int Client_Server_Disconnect(SOCKET& socketServer, SOCKADDR_IN& clieAddr, int& clieAddrlen)
{
    Packet_Header packet;
    char* buffer = new char[sizeof(packet)];  // 发送buffer
    int size = sizeof(packet);

    // 接收客户端发来的第一次挥手信息
    while (1) {
        if (recvfrom(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, &clieAddrlen) == -1) {
            cout << "无法接收客户端发送的连接请求" << endl;
            return 0;
        }
        memcpy(&packet, buffer, sizeof(packet));
        if (packet.tag == FIN_ACK && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "成功接收第一次挥手信息" << endl;
            break;
        }
    }

    // 第二次：服务端向客户端发送挥手信息
    packet.tag = ACK;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen) == -1)
    {
        return 0;
    }

    cout << "成功发送第二次挥手信息" << endl;
    //Sleep(10000);

    // 第三次：服务端向客户端发送挥手信息
    packet.tag = FIN_ACK;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen) == -1)
    {
        return 0;
    }
    clock_t start = clock();//记录第三次挥手发送时间

    // 第四次：服务端接收客户端发送的挥手信息
    while (recvfrom(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, &clieAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
        {
            sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen);
            start = clock();
            cout << "第三次挥手超时，正在进行重传" << endl;
        }
    }

    cout << "成功发送第三次挥手信息" << endl;

    // 对接收数据进行校验和检验
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "成功收到第四次挥手信息" << endl;
    }
    else {
        cout << "无法接收客户端回传建立可靠连接" << endl;
        return 0;
    }

    cout << "客户端与服务端成功断开连接！" << endl;

    return 1;
}


// 接收数据
int Recv_Messsage(SOCKET& socketServer, SOCKADDR_IN& clieAddr, int& clieAddrlen, char* Mes, int MAX_SIZE)
{
    Packet_Header packet;
    char* buffer = new char[sizeof(packet) + MAX_SIZE];  // 接收buffer
    int ack = 1;  // 确认序列号
    int seq = 0;
    long F_Len = 0;     //数据总长
    int S_Len = 0;      //单次数据长度


    // 接收文件
    while (1) {
        while (recvfrom(socketServer, buffer, sizeof(packet) + MAX_SIZE, 0, (sockaddr*)&clieAddr, &clieAddrlen) <= 0);
        memcpy(&packet, buffer, sizeof(packet));

        /*检验代码的正确性与应用性。确保如若发生超时，
        发送方& 接收方可以正确处理数据，
        保证数据不会出现错误失序、缺失的情况*/
        if (((rand() % (255 - 1)) + 1) == 199) {
            cout << int(packet.seq) << "随机超时重传测试" << endl;
            continue;
        }

        // END 全局结束
        if (packet.tag == END && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "The all_end tag has been recved" << endl;
            return 999;
        }

        // 结束标记
        if (packet.tag == OVER && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "The end tag has been recved" << endl;
            break;
        }

        // 接收数据
        if (packet.tag == 0 && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {

            // 对接收数据加以确认，否则发送标记告知客户端需要重发
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

            // 成功收到数据
            S_Len = packet.datasize;
            cout << "Begin recving message...... datasize:" << S_Len << " bytes!" << " flag:"
                << int(packet.tag) << " seq:" << int(packet.seq) << " checksum:" << int(packet.checksum) << endl;
            memcpy(Mes + F_Len, buffer + sizeof(packet), S_Len);
            F_Len += S_Len;

            // 返回 告知客户端已成功收到
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

    // 发送结束标志
    packet.tag = OVER;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    sendto(socketServer, buffer, sizeof(packet), 0, (sockaddr*)&clieAddr, clieAddrlen);
    cout << "The end tag has been sent" << endl;
    return F_Len;
}


// 服务端发送文件数据
void Send_Message(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* Message, int mes_size, int MAX_SIZE)
{
    int packet_num = mes_size / (MAX_SIZE)+(mes_size % MAX_SIZE != 0);
    int Seq_num = 0;  //初始化序列号
    Packet_Header packet;
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 非阻塞模式

    for (int i = 0; i < packet_num; i++)
    {
        int data_len = (i == packet_num - 1 ? mes_size - (packet_num - 1) * MAX_SIZE : MAX_SIZE);
        char* buffer = new char[sizeof(packet) + data_len];  // 发送buffer
        packet.tag = 0;
        packet.seq = Seq_num;
        packet.datasize = data_len;
        packet.checksum = 0;
        packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
        memcpy(buffer, &packet, sizeof(packet));
        char* mes = Message + i * MAX_SIZE;
        memcpy(buffer + sizeof(packet), mes, data_len);

        // 发送数据
        sendto(socketClient, buffer, sizeof(packet) + data_len, 0, (sockaddr*)&servAddr, servAddrlen);
        cout << "Begin sending message...... datasize:" << data_len << " bytes!" << " flag:"
            << int(packet.tag) << " seq:" << int(packet.seq) << " checksum:" << int(packet.checksum) << endl;

        clock_t start = clock();    //记录发送时间

        while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
            if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
            {
                sendto(socketClient, buffer, sizeof(packet) + data_len, 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "数据发送超时，正在重传！！！" << endl;
                start = clock();
            }
        }

        memcpy(&packet, buffer, sizeof(packet));
        if (packet.ack == (Seq_num + 1) % (256) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "Successful send and recv response  ack:" << int(packet.ack) << endl;
        }
        else {
            // 服务端未接受到数据 重传数据
            // 服务端接收到的数据 校验和出错 需要重传
            if (packet.ack == Seq_num || (compute_sum((WORD*)&packet, sizeof(packet)) != 0)) {
                cout << "客户端未接受到数据，需要接收重传数据！！！" << endl;
                i--;
                continue;
            }
            else {
                cout << "无法接收客户端回传" << endl;
                return;
            }
        }

        // Seq_num在 0-255之间
        Seq_num++;
        Seq_num = (Seq_num > 255 ? Seq_num - 256 : Seq_num);
    }

    //发送结束标志
    packet.tag = OVER;
    char* buffer = new char[sizeof(packet)];
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "The end tag has been sent" << endl;

    clock_t start = clock();

    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
        {
            sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
            cout << "数据发送超时，正在重传！！！" << endl;
            start = clock();
        }
    }

    memcpy(&packet, buffer, sizeof(packet));
    if (packet.tag == OVER && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "The end token was successfully received" << endl;
    }
    else {
        cout << "无法接收客户端回传" << endl;
        return;
    }
    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 阻塞模式
    return;
}


/////////////////////////////////////////////////////////////////////////////////////////////////
// ====================================== mian ==================================================
/////////////////////////////////////////////////////////////////////////////////////////////////


// 功能包括：建立连接、差错检测、确认重传等。
// 流量控制采用停等机制

int main()
{
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0)
    {
        cout << "Startup 出错！" << endl;
        WSACleanup();
        return 0;
    }

    SOCKET server = socket(AF_INET, SOCK_DGRAM, 0);//创建服务端套接字
    if (server == -1)
    {
        cout << "socket 生成出错！" << endl;
        WSACleanup();
        return 0;
    }
    cout << "服务端套接字创建成功！" << endl;

    // 绑定端口和ip
    SOCKADDR_IN addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &addr.sin_addr.s_addr);

    // 建立连接
    if (bind(server, (SOCKADDR*)&addr, sizeof(addr)) != 0)
    {
        cout << "bind 出错！" << endl;
        WSACleanup();
        return 0;
    }
    int length = sizeof(addr);
    cout << "等待客户端提出连接请求......" << endl;
    int label = Client_Server_Connect(server, addr, length);
    cout << "请输入本端数据缓冲区大小：";
    int SIZE;
    cin >> SIZE;
    SIZE = Client_Server_Size(server, addr, length, SIZE);

    if (label == 0) {
        while (1) {
            cout << "=====================================================================" << endl;
            cout << "Waiting to receive!!!" << endl;
            char* F_name = new char[20];
            char* Message = new char[100000000];
            int name_len = Recv_Messsage(server, addr, length, F_name, SIZE);

            // 全局结束
            if (name_len == 999)
                break;

            int file_len = Recv_Messsage(server, addr, length, Message, SIZE);
            string a;
            for (int i = 0; i < name_len; i++) {
                a = a + F_name[i];
            }
            cout << "接收文件名：" << a << endl;
            cout << "接收文件数据大小：" << file_len << "bytes!" << endl;

            FILE* fp;
            if ((fp = fopen(a.c_str(), "wb")) == NULL)
            {
                cout << "Open File failed!" << endl;
                exit(0);
            }
            //从buffer中写数据到fp指向的文件中
            fwrite(Message, file_len, 1, fp);
            cout << "Successfully receive the data in its entirety and save it" << endl;
            cout << "=====================================================================" << endl;
            //关闭文件指针
            fclose(fp);
        }
    }
    if (label == 1) {
        while (1) {
            cout << "=====================================================================" << endl;
            cout << "Waiting to choose File!!!" << endl;
            //输入文件名
            char InFileName[20];
            //文件数据长度
            int F_length;
            //输入要读取的图像名
            cout << "Enter File name:";
            cin >> InFileName;

            // 全局结束
            if (InFileName[0] == 'q' && strlen(InFileName) == 1) {
                Packet_Header packet;
                char* buffer = new char[sizeof(packet)];  // 发送buffer
                packet.tag = END;
                packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
                memcpy(buffer, &packet, sizeof(packet));
                sendto(server, buffer, sizeof(packet), 0, (sockaddr*)&addr, length);
                cout << "Send a all_end flag to the server" << endl;
                break;
            }

            //文件指针
            FILE* fp;
            //以二进制方式打开图像
            if ((fp = fopen(InFileName, "rb")) == NULL)
            {
                cout << "Open file failed!" << endl;
                exit(0);
            }
            //获取文件数据总长度
            fseek(fp, 0, SEEK_END);
            F_length = ftell(fp);
            rewind(fp);
            //根据文件数据长度分配内存buffer
            char* FileBuffer = (char*)malloc(F_length * sizeof(char));
            //将图像数据读入buffer
            fread(FileBuffer, F_length, 1, fp);
            fclose(fp);

            cout << "发送文件数据大小：" << F_length << "bytes!" << endl;

            Send_Message(server, addr, length, InFileName, strlen(InFileName), SIZE);
            clock_t start = clock();
            Send_Message(server, addr, length, FileBuffer, F_length, SIZE);
            clock_t end = clock();
            cout << "传输总时间为:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
            cout << "吞吐率为:" << ((float)F_length) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;
            cout << "=====================================================================" << endl;
        }
    }
    Client_Server_Disconnect(server, addr, length);
    closesocket(server);
    WSACleanup();
    system("pause");
    return 0;
}