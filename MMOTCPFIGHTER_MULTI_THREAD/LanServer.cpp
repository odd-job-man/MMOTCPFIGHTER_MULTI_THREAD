#include <crtdbg.h>
#include <process.h>
#include <stdio.h>
#include <WinSock2.h>
#include <windows.h>
#include "Logger.h"
#include "IHandler.h"
#include "Stack.h"
#include "Packet.h"
#include "LanServer.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"LoggerMt.lib")

//#define GQCSRET
//#define SENDRECV
//#define WILL_SEND
//#define WILL_RECV
//#define SESSION_DELETE

int g_iCount = 0;

ULONGLONG g_recv = 0;
ULONGLONG g_send = 0;

unsigned __stdcall IOCPWorkerThread(LPVOID arg);
unsigned __stdcall AcceptThread(LPVOID arg);

DWORD g_dwID;

#define SERVERPORT 11402 

#define ZERO_BYTE_SEND
#define LINGER

SOCKET g_ListenSock;


#define RECV_COMPL 1000
#define SEND_COMPL 1001

#define LOGIN_PAYLOAD ((ULONGLONG)0x7fffffffffffffff)

BOOL LanServer::Start(DWORD dwMaxSession)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, GetStdHandle(STD_OUTPUT_HANDLE));

	int retval;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp Fail ErrCode : %u",WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp OK!");
	// NOCT�� 0���� �����μ��� ����ŭ�� ������
	hcp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (!hcp_)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"CreateIoCompletionPort Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	hListenSock_ = socket(AF_INET, SOCK_STREAM, 0);
	if (hListenSock_== INVALID_SOCKET)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET OK");

	// bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(hListenSock_, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind OK");

	// listen
	retval = listen(hListenSock_, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen() OK");

#ifdef LINGER
	linger linger;
	linger.l_linger = 0;
	linger.l_onoff = 1;
	setsockopt(hListenSock_, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"linger() OK");
#endif

#ifdef ZERO_BYTE_SEND
	DWORD dwSendBufSize = 0;
	setsockopt(hListenSock_, SOL_SOCKET, SO_SNDBUF, (char*)&dwSendBufSize, sizeof(dwSendBufSize));
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Zerobyte Send OK");
#endif

	hIOCPWorkerThreadArr_ = new HANDLE[si.dwNumberOfProcessors * 2];
	for (DWORD i = 0; i < si.dwNumberOfProcessors * 2; ++i)
	{
		hIOCPWorkerThreadArr_[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, nullptr);
		if (!hIOCPWorkerThreadArr_[i])
		{
			LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE WorkerThread Fail ErrCode : %u", WSAGetLastError());
			__debugbreak();
		}
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", si.dwNumberOfProcessors);

	pSessionArr_ = new Session[dwMaxSession];
	lMaxSession_ = dwMaxSession;
	DisconnectStack_.Init(dwMaxSession, sizeof(SHORT));
	for (int i = dwMaxSession - 1; i >= 0; --i)
	{
		DisconnectStack_.Push((void**)&i);
		pSessionArr_[i].bUsing = FALSE;
	}
	InitializeCriticalSection(&stackLock_);

	hAcceptThread_ = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, nullptr);
	if (!hAcceptThread_)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE AccpetThread Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE AccpetThread OK!");
	return TRUE;
}

__forceinline void ClearPacket(Session* pSession)
{
	DWORD dwSendBufNum = _InterlockedExchange(&pSession->lSendBufNum, 0);
	for (DWORD i = 0; i < dwSendBufNum; ++i)
	{
		Packet* pPacket;
		pSession->sendRB.Dequeue((char*)&pPacket, sizeof(Packet*));
		Packet::Free(pPacket);
	}
}

__forceinline void ReleaseSendFailPacket(Session* pSession)
{
	DWORD dwSendBufNum = pSession->sendRB.GetUseSize() / sizeof(Packet*);
	for (DWORD i = 0; i < dwSendBufNum; ++i)
	{
		Packet* pPacket;
		pSession->sendRB.Dequeue((char*)&pPacket, sizeof(Packet*));
		Packet::Free(pPacket);
	}
}

unsigned __stdcall LanServer::IOCPWorkerThread(LPVOID arg)
{
	LanServer* pLanServer = (LanServer*)arg;
	while (1)
	{
		WSAOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		Session* pSession = nullptr;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pLanServer->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);

#ifdef GQCSRET
		LOG(L"DEBUG", DEBUG, TEXTFILE, L"Thread ID : %u, GQCS return Session ID : %llu, IoCount : %d", GetCurrentThreadId(), pSession->id.ullId, InterlockedExchange((LONG*)&pSession->IoCnt, pSession->IoCnt));
#endif
		do
		{
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			//��������
			if (bGQCSRet && dwNOBT == 0)
				break;

			//������ ����
			//�α��� �Ϸ������� GQCS ���� WSAGetLastError ���� 64�� ���� �������.
			//���� WSASend�� WSARecv, �� �� �ϳ��� �ٷ� �����ϴ� ��쿡�� �α��ϴ°�����...
			if (!bGQCSRet && pOverlapped)
				break;

			if (&pSession->recvOverlapped == pOverlapped)
				pLanServer->RecvProc(pSession, dwNOBT);
			else
				pLanServer->SendProc(pSession, dwNOBT);

		} while (0);

		if (InterlockedDecrement(&pSession->IoCnt) == 0)
			pLanServer->ReleaseSession(pSession);
	}
}

unsigned __stdcall LanServer::AcceptThread(LPVOID arg)
{
	SOCKET clientSock;
	SOCKADDR_IN clientAddr;
	int addrlen;
	LanServer* pLanServer = (LanServer*)arg;
	addrlen = sizeof(clientAddr);

	while (1)
	{
		clientSock = accept(pLanServer->hListenSock_, (SOCKADDR*)&clientAddr, &addrlen);

		if (clientSock == INVALID_SOCKET)
		{
			DWORD dwErrCode = WSAGetLastError();
			if (dwErrCode != WSAEINTR && dwErrCode != WSAENOTSOCK)
			{
				__debugbreak();
			}
			return 0;
		}


		if (!pLanServer->OnConnectionRequest())
		{
			closesocket(clientSock);
			continue;
		}

		InterlockedIncrement((LONG*)&pLanServer->lAcceptTPS_);
		InterlockedIncrement((LONG*)&pLanServer->lSessionNum_);

		SHORT shIndex;
		EnterCriticalSection(&pLanServer->stackLock_);
		pLanServer->DisconnectStack_.Pop((void**)&shIndex);
		LeaveCriticalSection(&pLanServer->stackLock_);
		Session* pSession = pLanServer->pSessionArr_ + shIndex;
		pSession->Init(clientSock, g_dwID, shIndex);
		CreateIoCompletionPort((HANDLE)pSession->sock, pLanServer->hcp_, (ULONG_PTR)pSession, 0);
		++g_dwID;

		InterlockedIncrement(&pSession->IoCnt);
		pLanServer->OnAccept(pSession->id);
		pLanServer->RecvPost(pSession);
		if (InterlockedDecrement(&pSession->IoCnt) == 0)
			pLanServer->ReleaseSession(pSession);

	}
}

BOOL LanServer::OnConnectionRequest()
{
	if (lSessionNum_ + 1 >= lMaxSession_)
		return FALSE;

	return TRUE;
}

void* LanServer::OnAccept(ID id)
{
	//Packet* pPacket = Packet::Alloc();
	//(*pPacket) << LOGIN_PAYLOAD;
	//SendPacket(id, pPacket);
	return nullptr;
}

void LanServer::OnRecv(ID id, Packet* pPacket)
{
//	ULONGLONG ullPayLoad;
//	(*pPacket) >> ullPayLoad;
//
//	Packet* pSendPacket = Packet::Alloc();
//	(*pSendPacket) << ullPayLoad;
//	SendPacket(id, pSendPacket);
}

void LanServer::Monitoring()
{
	printf(
		"Accept TPS: %d\n"
		"Disconnect TPS: %d\n"
		"Recv Msg TPS: %d\n"
		"Send Msg TPS: %d\n"
		"User Num : %d\n\n",
		lAcceptTPS_, lDisconnectTPS_, lRecvTPS_, lSendTPS_, lSessionNum_);

	lAcceptTPS_ = lDisconnectTPS_ = lRecvTPS_ = lSendTPS_ = 0;
}

void LanServer::Stop()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	closesocket(hListenSock_);
	WaitForSingleObject(hAcceptThread_, INFINITE);

	for (int i = 0; i < lMaxSession_; ++i)
	{
		CancelIoEx((HANDLE)pSessionArr_[i].sock, nullptr);
	}

	while (InterlockedExchange(&lSessionNum_, lSessionNum_) > 0);

	for (DWORD i = 0; i < si.dwNumberOfProcessors * 2; ++i)
	{
		PostQueuedCompletionStatus(hcp_, 0, 0, 0);
	}

	WaitForMultipleObjects(si.dwNumberOfProcessors * 2, hIOCPWorkerThreadArr_, TRUE, INFINITE);
	for (DWORD i = 0; i < si.dwNumberOfProcessors * 2; ++i)
	{
		CloseHandle(hIOCPWorkerThreadArr_[i]);
	}
	delete[] hIOCPWorkerThreadArr_;

	for (DWORD i = 0; i < si.dwNumberOfProcessors * 2; ++i)
	{
		if (InterlockedExchange((LONG*)&pSessionArr_[i].bUsing, pSessionArr_[i].bUsing) == TRUE)
			__debugbreak();
	}
	delete[] pSessionArr_;
	Packet::ReleasePacketPool();
	DisconnectStack_.Clear();
	CloseHandle(hcp_);
	DeleteCriticalSection(&stackLock_);
	WSACleanup();
}

void LanServer::SendPacket(ID id, Packet* pPacket)
{
	Session* pSession = pSessionArr_ + GET_SESSION_INDEX(id);
	if (pSession->id.ullId == id.ullId && pSession->bUsing)
	{
		NET_HEADER* pNetHeader = (NET_HEADER*)pPacket->pBuffer_;
		pNetHeader->byCode = (BYTE)0x89;
		pNetHeader->byLen = pPacket->GetUsedDataSize() - 1;
		pSession->sendRB.Enqueue((const char*)&pPacket, sizeof(pPacket));
		SendPost(pSession);
	}
}

BOOL LanServer::RecvPost(Session* pSession)
{
	DWORD flags;
	int iRecvRet;
	DWORD dwErrCode;
	WSABUF wsa[2];

	wsa[0].buf = pSession->recvRB.GetWriteStartPtr();
	wsa[0].len = pSession->recvRB.DirectEnqueueSize();
	wsa[1].buf = pSession->recvRB.Buffer_;
	wsa[1].len = pSession->recvRB.GetFreeSize() - wsa[0].len;

	ZeroMemory(&pSession->recvOverlapped, sizeof(WSAOVERLAPPED));
	flags = 0;
	InterlockedIncrement(&pSession->IoCnt);
	iRecvRet = WSARecv(pSession->sock, wsa, 2, nullptr, &flags, &(pSession->recvOverlapped), nullptr);
	if (iRecvRet == SOCKET_ERROR)
	{
		dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
			return TRUE;

		InterlockedDecrement(&(pSession->IoCnt));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

BOOL LanServer::SendPost(Session* pSession)
{
	// ���� ���� TRUE�� �ٲ۴�. ���� TRUE���ٸ� ��ȯ���� TRUE�ϰ��̸� �׷��ٸ� ���� SEND �������̱� ������ �׳� ����������
	// �� ���ǹ��� ��ġ�� ���Ͽ� Out�� �ٲ��� ���������� ����ȴ�.
	// ������ SendPost ������ü�� Send�Ϸ����� �������� ��쿡�� in�� ��ġ�� SendPacket���� ���ؼ� �ٲ���� �ִ�.
	// iUseSize�� ���ϴ� ���������� DirectDequeueSize�� ���� �޶������ִ�.
	if (InterlockedExchange((LONG*)&pSession->bSendingInProgress, TRUE) == TRUE)
		return TRUE;

	// SendPacket���� in�� �Űܼ� UseSize�� 0���� Ŀ���������� Send�Ϸ������� �����ؼ� Out�� �ű�� �÷��� ���� Recv�Ϸ����� �����尡 ���� SendPost�� ������ �÷��׸� �����Ѱ�� UseSize�� 0�̳��´�.
	// ���⼭ flag�� �ٽ� FALSE�� �ٲپ����� �ʾƼ� ����߻�
	int out = pSession->sendRB.iOutPos_;
	int in = pSession->sendRB.iInPos_;
	int iUseSize;
	GetUseSize_MACRO(in, out, iUseSize);

	if (iUseSize == 0)
	{
		InterlockedExchange((LONG*)&pSession->bSendingInProgress, FALSE);
		return TRUE;
	}

	WSABUF wsa[50];
	DWORD i;
	DWORD dwBufferNum = iUseSize / sizeof(Packet*);
	for (i = 0; i < 50 && i < dwBufferNum; ++i)
	{
		Packet* pPacket;
		pSession->sendRB.PeekAt((char*)&pPacket, out, sizeof(Packet*));
		wsa[i].buf = (char*)pPacket->pBuffer_;
		wsa[i].len = pPacket->GetUsedDataSize() + sizeof(NET_HEADER);
		MoveInOrOutPos_MACRO(out, sizeof(Packet*));
	}

	InterlockedExchange(&pSession->lSendBufNum, i);
	InterlockedAdd(&lSendTPS_, i);
	InterlockedIncrement(&pSession->IoCnt);
	ZeroMemory(&(pSession->sendOverlapped), sizeof(WSAOVERLAPPED));
#ifdef WILL_SEND 
	LOG(L"DEBUG", DEBUG, TEXTFILE, L"Thread ID : %u, SendPost WSASend Session ID : %llu, IoCount : %d", GetCurrentThreadId(), pSession->id.ullId, InterlockedExchange((LONG*)&pSession->IoCnt, pSession->IoCnt));
#endif
	int iSendRet = WSASend(pSession->sock, wsa, i, nullptr, 0, &(pSession->sendOverlapped), nullptr);
	if (iSendRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
			return TRUE;

		InterlockedExchange((LONG*)&pSession->bSendingInProgress, FALSE);
		InterlockedDecrement(&(pSession->IoCnt));
#ifdef WILL_SEND 
		LOG(L"DEBUG", DEBUG, TEXTFILE, L"Thread ID : %u, WSASend Fail Session ID : %llu, IoCount : %d", GetCurrentThreadId(), pSession->id.ullId, InterlockedExchange((LONG*)&pSession->IoCnt, pSession->IoCnt));
#endif
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

void LanServer::ReleaseSession(Session* pSession)
{
#ifdef SESSION_DELETE 
	LOG_ASYNC(L"Delete Session : %llu", pSession->id.ullId);
#endif
	ReleaseSendFailPacket(pSession);
	closesocket(pSession->sock);
	InterlockedExchange((LONG*)&pSession->bUsing, FALSE);
	SHORT shIndex = (SHORT)(pSession - pSessionArr_);
	if (pSession->sendRB.GetUseSize() > 0)
		__debugbreak();

#ifdef _DEBUG
	_ASSERTE(_CrtCheckMemory());
#endif
	EnterCriticalSection(&stackLock_);
	DisconnectStack_.Push((void**)&shIndex);
	LeaveCriticalSection(&stackLock_);
	InterlockedIncrement(&lDisconnectTPS_);
	InterlockedDecrement(&lSessionNum_);
}

void LanServer::RecvProc(Session* pSession, DWORD dwNumberOfByteTransferred)
{
#ifdef SENDRECV
	LOG(L"DEBUG", DEBUG, TEXTFILE, L"Thread ID : %u, Recv Complete Session ID : %llu", GetCurrentThreadId(), pSession->id.ullId);
#endif
	NET_HEADER header;
	Packet* pPacket = Packet::Alloc();
	pSession->recvRB.MoveInPos(dwNumberOfByteTransferred);
	while (1)
	{
		if (pSession->recvRB.Peek((char*)&header, sizeof(header)) == 0)
			break;

		if (header.byCode != 0x89)
			__debugbreak();

		if (pSession->recvRB.GetUseSize() < sizeof(header) + header.byLen + sizeof(BYTE))
			break;

		pSession->recvRB.MoveOutPos(sizeof(header));
		pSession->recvRB.Dequeue(pPacket->GetBufferPtr(), header.byLen + sizeof(BYTE));
		pPacket->MoveWritePos(header.byLen + sizeof(BYTE));
		OnRecv(pSession->id, pPacket);
		pPacket->Clear();
		InterlockedIncrement(&lRecvTPS_);
	}
	RecvPost(pSession);
	Packet::Free(pPacket);
}

void LanServer::SendProc(Session* pSession, DWORD dwNumberOfBytesTransferred)
{
	ClearPacket(pSession);
	InterlockedExchange((LONG*)&pSession->bSendingInProgress, FALSE);

	if (pSession->sendRB.GetUseSize() > 0)
		SendPost(pSession);
}






