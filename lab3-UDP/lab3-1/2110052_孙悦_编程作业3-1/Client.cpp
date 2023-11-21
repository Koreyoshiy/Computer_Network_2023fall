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
// ==============================================格式===============================================
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
typedef struct Packet_Header
{
    WORD datasize;		// 数据长度
    BYTE tag;			// 标签
                        //八位，使用后四位，排列是OVER FIN ACK SYN 
    BYTE window;		// 窗口大小 unused
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==================================================各类函数=========================================================
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 计算校验和
WORD compute_sum(WORD* message, int size) {   // size = 8
    int count = (size + 1) / 2;  // 防止奇数字节数 确保WORD 16位
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

// 客户端与服务端建立连接（采取三次握手的方式）
int Client_Server_Connect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, int label)
{
    Packet_Header packet;
    int size = sizeof(packet);  // size = 8
    char* buffer = new char[sizeof(packet)];  // 发送buffer

    // 第一次：客户端首先向服务端发送建立连接请求
    packet.tag = SYN;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto函数发送数据出错！" << endl;
        return 0;
    }

    // 设定超时重传机制
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 非阻塞模式
    clock_t start = clock(); //记录发送第一次握手时间

    // 第二次：客户端接收服务端回传的握手
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
        {
            sendto(socketClient, buffer, size, 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "第一次握手超时，正在进行重传" << endl;
        }
    }

    cout << "成功发送第一次握手信息" << endl;

    // 对接收数据进行校验和检验
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "成功收到第二次握手信息" << endl;
    }
    else {
        cout << "无法接收服务端回传建立可靠连接" << endl;
        return 0;
    }

    // 第三次：客户端发送第三次握手数据
    packet.tag = ACK_SYN;
    packet.datasize = label;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto函数发送数据出错！" << endl;
        return 0;
    }

    cout << "成功发送第三次握手信息" << endl;
    cout << "客户端与服务端成功进行三次握手建立连接！---------可以开始发送/接收数据" << endl;

    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);//设为阻塞模式
    return 1;
}

int Client_Server_Size(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, int Size) {
    Packet_Header packet;
    int size = sizeof(packet);  // size = 8
    char* buffer = new char[sizeof(packet)];  // 发送buffer

    // 告知服务端 客户端的缓冲区为多大
    packet.tag = SYN;
    packet.datasize = Size;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto函数发送数据出错！" << endl;
        return 0;
    }

    // 设定超时重传机制
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 非阻塞模式
    clock_t start = clock(); //记录发送第一次握手时间

    // 接收服务端回传告知的缓冲区大小
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
        {
            sendto(socketClient, buffer, size, 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "数据发送超时！正在进行重传" << endl;
        }
    }

    cout << "成功告知对方缓冲区大小" << endl;

    // 对接收数据进行校验和检验
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "成功收到对方缓冲区大小信息" << endl;
    }
    else {
        cout << "无法接收服务端回传建立可靠连接" << endl;
        return 0;
    }

    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 阻塞模式

    if (Size > packet.datasize)
        return packet.datasize;
    else
        return Size;
}

// 客户端与服务器端断开连接（采取四次挥手的方式）
int Client_Server_Disconnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
    Packet_Header packet;
    char* buffer = new char[sizeof(packet)];  // 发送buffer
    int size = sizeof(packet);

    // 客户端第一次发起挥手
    packet.tag = FIN_ACK;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, size);
    memcpy(buffer, &packet, size);
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto函数发送数据出错！" << endl;
        return 0;
    }

    clock_t start = clock();   // 记录第一次挥手的时间

    // 客户端接收服务端发来的第二次挥手
    while (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
        if ((clock() - start) / 1000 > 1)   //时间超过1秒 标记为超时重传 
        {
            sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "第一次握手超时，正在进行重传" << endl;
        }
    }

    cout << "成功发送第一次挥手信息" << endl;

    // 对接收数据进行校验和检验
    memcpy(&packet, buffer, sizeof(packet));
    if ((packet.tag == ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
        cout << "成功收到第二次挥手信息" << endl;
    }
    else {
        cout << "无法接收服务端回传断开连接" << endl;
        return 0;
    }

    // 客户端接收服务端发来的第三次挥手
    while (1) {
        if (recvfrom(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, &servAddrlen) == -1) {
            cout << "无法接收服务端回传断开连接" << endl;
            return 0;
        }
        // 对接收数据进行校验和检验
        memcpy(&packet, buffer, sizeof(packet));
        if ((packet.tag == FIN_ACK) && (compute_sum((WORD*)&packet, sizeof(packet)) == 0)) {
            cout << "成功收到第三次挥手信息" << endl;
            break;
        }
    }

    // 第四次：客户端发送第四次挥手数据
    packet.tag = ACK;
    packet.checksum = 0;
    packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
    memcpy(buffer, &packet, sizeof(packet));
    if (sendto(socketClient, buffer, sizeof(packet), 0, (sockaddr*)&servAddr, servAddrlen) == -1) {
        cout << "sendto函数发送数据出错！" << endl;
        return 0;
    }

    cout << "成功发送第四次挥手信息" << endl;
    cout << "客户端与服务端成功断开连接！" << endl;

    return 1;
}

// 发送数据包

void Send_Message(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* Message, int mes_size, int MAX_SIZE)
{
    int packet_num = mes_size / (MAX_SIZE)+(mes_size % MAX_SIZE != 0); //计算需要分成多少个数据包来发送整个消息。
    int Seq_num = 0;  //初始化序列号，用于标识数据包的顺序
    Packet_Header packet;//存储数据包的头部信息
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 非阻塞模式

    for (int i = 0; i < packet_num; i++)
    {
        // 计算当前数据包的长度
        int data_len = (i == packet_num - 1 ? mes_size - (packet_num - 1) * MAX_SIZE : MAX_SIZE);
        // 分配发送缓冲区
        char* buffer = new char[sizeof(packet) + data_len];  // 发送buffer
         // 设置数据包的头部信息
        packet.tag = 0;
        packet.seq = Seq_num;
        packet.datasize = data_len;
        packet.checksum = 0;
        packet.checksum = compute_sum((WORD*)&packet, sizeof(packet));
        memcpy(buffer, &packet, sizeof(packet));
        // 将消息数据复制到缓冲区
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
                cout << "服务端未接受到数据，正在重传！！！" << endl;
                i--;
                continue;
            }
            else {
                cout << "无法接收服务端回传" << endl;
                return;
            }
        }
        //更新序列号
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
        cout << "无法接收服务端回传" << endl;
        return;
    }
    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);  // 阻塞模式
    return;
}


// 客户端接收数据
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//=========================================== main =====================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





// 功能包括：建立连接、差错检测、确认重传等。
// 流量控制采用停等机制

int main()
{
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        cout << "Startup 出错！" << endl;
        WSACleanup();
        return 0;
    }
    SOCKET client = socket(AF_INET, SOCK_DGRAM, 0);
    if (client == -1)
    {
        cout << "socket 生成出错！" << endl;
        WSACleanup();
        return 0;
    }
    cout << "客户端套接字创建成功！" << endl;

    // 填充信息
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &serveraddr.sin_addr.s_addr);

    // 输入选项
    cout << "请选择——（发送0 / 接收1）数据：";
    int label;
    cin >> label;

    // 建立连接
    int length = sizeof(serveraddr);
    cout << "开始向服务端建立连接请求......" << endl;
    Client_Server_Connect(client, serveraddr, length, label);

    // 输入缓冲区大小
    cout << "请输入本端数据缓冲区大小：";
    int SIZE;
    cin >> SIZE;
    SIZE = Client_Server_Size(client, serveraddr, length, SIZE);

    if (label == 0) {
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
                sendto(client, buffer, sizeof(packet), 0, (sockaddr*)&serveraddr, length);
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

            Send_Message(client, serveraddr, length, InFileName, strlen(InFileName), SIZE);
            clock_t start = clock();
            Send_Message(client, serveraddr, length, FileBuffer, F_length, SIZE);
            clock_t end = clock();
            cout << "传输总时间为:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
            cout << "吞吐率为:" << ((float)F_length) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;
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

            // 全局结束
            if (name_len == 999)
                break;

            int file_len = Recv_Messsage(client, serveraddr, length, Message, SIZE);
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
    Client_Server_Disconnect(client, serveraddr, length);
    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}