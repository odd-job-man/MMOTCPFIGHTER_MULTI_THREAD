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
	void* pPlayer;
	LONG lSendBufNum;
	LONG IoCnt;
	WSAOVERLAPPED recvOverlapped;
	WSAOVERLAPPED sendOverlapped;
	SRWLOCK sendRBLock;
	RingBuffer recvRB;
	RingBuffer sendRB;
	Session()
	{
		InitializeSRWLock(&sendRBLock);
	}
	BOOL Init(SOCKET clientSock, DWORD dwClientID, SHORT shIdx);
#ifdef IO_RET
	ULONGLONG ullSend;
	ULONGLONG ullRecv;
#endif
};
