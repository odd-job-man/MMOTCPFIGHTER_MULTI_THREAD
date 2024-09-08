#pragma once
#include "RingBuffer.h"
#include "ID.h"

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
	BOOL Init(SOCKET clientSock, DWORD dwClientID, SHORT shIdx);
#ifdef IO_RET
	ULONGLONG ullSend;
	ULONGLONG ullRecv;
#endif
};
