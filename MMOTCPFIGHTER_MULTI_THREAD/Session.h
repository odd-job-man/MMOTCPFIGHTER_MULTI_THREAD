#pragma once
#include "RingBuffer.h"
#include "ID.h"

//#define IO_RET

#define GET_SESSION_INDEX(id) (id.ullId & 0xFFFF)
#define MAKE_SESSION_INDEX(Ret,ullID,index)\
do{\
Ret.ullId = Ret.ullId << 16;\
Ret.ullId ^= index;\
}while(0)\

struct Session
{
	SOCKET sock;
	ID id;
	BOOL bSendingInProgress;
	BOOL bUsing;
	DWORD dwLastRecvTime;
	void* pClient;
	LONG lSendBufNum;
	LONG IoCnt;
	WSAOVERLAPPED recvOverlapped;
	WSAOVERLAPPED sendOverlapped;
	RingBuffer recvRB;
	RingBuffer sendRB;
	BOOL Init(SOCKET clientSock, ULONGLONG ullClientID, SHORT shIdx);
#ifdef IO_RET
	ULONGLONG ullSend;
	ULONGLONG ullRecv;
#endif
};
