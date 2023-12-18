#pragma once
#include<iostream>
#include<winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")  

// BYTE  : unsigned char
// WORD  : unsigned short
// DWOED : unsigned long

#define max_size 10240
//消息格式
class message {
public:
	WORD src_port; //源端口
	WORD dest_port; //目的端口
	DWORD length; //长度
	WORD checksum; //校验和
	DWORD Seq; // 序列号
	DWORD Ack; //确认号
	WORD flag;  // 标志位 0:SYN 1:ACK 2:FIN  3:EOF
	char data[max_size]; //存储消息的数据部分，可以存储文件内容等信息
	message(); //构造函数，用于初始化 message 对象
	void reset(); //重置消息对象的所有成员，将它们全部置零
	void setport(WORD src, WORD dest); //设置源端口和目的端口
	void setlen(DWORD len); //设置消息的长度
	void setcheck(WORD check); //设置校验和
	void setSeq(DWORD seq); //设置消息的序列号
	void setAck(DWORD ack); //设置消息的确认号
	DWORD getSeq(); //获取消息的序列号
	DWORD getAck(); //获取消息的确认号
	//设置相应的标志位  &  检查相应的标志位是否被设置
	void setSYN() { flag = flag | 0x1; }
	bool checkSYN() { return flag & 0x1; }
	void setACK() { flag = flag | 0x02; }
	bool checkACK() { return flag & 0x02; }
	void setFIN() { flag = flag | 0x04; }
	bool checkFIN() { return flag & 0x04; }
	void setEOF() { flag = flag | 0x08; }
	bool checkEOF() { return flag & 0x08; }
};

message::message() {
	memset(this, 0, sizeof(message));
}

void message::reset() {
	memset(this, 0, sizeof(message));
}

void message::setport(WORD src, WORD dest) {
	this->src_port = src;
	this->dest_port = dest;
}

void message::setlen(DWORD len) {
	this->length = len;
}

void message::setcheck(WORD check) {
	this->checksum = check;
}

void message::setSeq(DWORD seq) {
	this->Seq = seq;
}

void message::setAck(DWORD ack) {
	this->Ack = ack;
}

DWORD message::getSeq() {
	return this->Seq;
}

DWORD message::getAck() {
	return this->Ack;
}

//伪首部
class pheader {
public:
	DWORD src_ip; //源IP
	DWORD dest_ip; //目的IP
	BYTE zero; //一个字节的零，用于填充，保证伪首部的总长度为12字节
	BYTE protocol; //表示传输层协议类型，通常为UDP的协议值（17）
	WORD length; //表示传输层协议数据部分的长度
	pheader();
	void setIP(DWORD src, DWORD dest); //设置源IP地址和目的IP地址
};

pheader::pheader() {
	memset(this, 0, sizeof(pheader));
}

void pheader::setIP(DWORD src, DWORD dest) {
	this->src_ip = src;
	this->dest_ip = dest;
}

//计算校验和
void CheckSum(message& a, pheader& b) {
	DWORD sum = 0;
	for (int i = 0; i < sizeof(message) / 2; i++) {
		sum += ((WORD*)&a)[i];
	}
	for (int i = 0; i < sizeof(pheader) / 2; i++) {
		sum += ((WORD*)&b)[i];
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	WORD temp = ~sum & 0xffff;
	a.setcheck(temp);
}

//检验收到的报文和伪首部
bool check(message& a, pheader& b) {
	DWORD sum = 0;
	for (int i = 0; i < sizeof(message) / 2; i++) {
		sum += ((WORD*)&a)[i];
	}
	for (int i = 0; i < sizeof(pheader) / 2; i++) {
		sum += ((WORD*)&b)[i];
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return sum == 0xffff;
}

void update(int& s, int& a, int len) {
	s = a;
	a = s + len;
}
