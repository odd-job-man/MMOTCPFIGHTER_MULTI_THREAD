#include <crtdbg.h>
#include <process.h>
#include <stdio.h>
#include <WinSock2.h>
#include <windows.h>
#include <time.h>
#include "Logger.h"
#include "IHandler.h"
#include "Stack.h"
#include "Packet.h"
#include "FreeList.h"
#include "GameServer.h"
#include "Client.h"
#include "Constant.h"
#include "Sector.h"
#include "SCCContents.h"
#include "CSCContents.h"
#include "Update.h"
#include "MemLog.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"LoggerMT")

FreeList<Player> ClientFreeList{ false,0 };
extern GameServer g_GameServer;

#define SERVERPORT 11402
#define LINGER
#define ZERO_BYTE_SEND

DWORD g_dwID;

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

unsigned __stdcall GameServer::AcceptThread(LPVOID arg)
{
	srand(time(nullptr));
	SOCKET clientSock;
	SOCKADDR_IN clientAddr;
	int addrlen;
	GameServer* pGameServer = (GameServer*)arg;
	addrlen = sizeof(clientAddr);

	while (1)
	{
		clientSock = accept(pGameServer->hListenSock_, (SOCKADDR*)&clientAddr, &addrlen);

		if (clientSock == INVALID_SOCKET)
		{
			DWORD dwErrCode = WSAGetLastError();
			if (dwErrCode != WSAEINTR && dwErrCode != WSAENOTSOCK)
			{
				__debugbreak();
			}
			return 0;
		}

		if (!pGameServer->OnConnectionRequest())
		{
			closesocket(clientSock);
			continue;
		}

		SHORT shIndex;
		EnterCriticalSection(&pGameServer->stackLock_);
		pGameServer->DisconnectStack_.Pop((void**)&shIndex);
		LeaveCriticalSection(&pGameServer->stackLock_);
		Session* pSession = pGameServer->pSessionArr_ + shIndex;
		pSession->Init(clientSock, g_dwID, shIndex);
		CreateIoCompletionPort((HANDLE)pSession->sock, pGameServer->hcp_, (ULONG_PTR)pSession, 0);
		++g_dwID;

		InterlockedIncrement(&pSession->IoCnt);
		pSession->pPlayer = pGameServer->OnAccept(pSession->id);
		pGameServer->RecvPost(pSession);
		if (InterlockedDecrement(&pSession->IoCnt) == 0)
			pGameServer->ReleaseSession(pSession);
	}
}

unsigned __stdcall GameServer::IOCPWorkerThread(LPVOID arg)
{
	GameServer* pGameServer = (GameServer*)arg;
	while (1)
	{
		WSAOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		Session* pSession = nullptr;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pGameServer->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		do
		{
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			//정상종료
			if (bGQCSRet && dwNOBT == 0)
				break;

			//비정상 종료
			//로깅을 하려햇으나 GQCS 에서 WSAGetLastError 값을 64로 덮어 써버린다.
			//따라서 WSASend나 WSARecv, 둘 중 하나가 바로 실패하는 경우에만 로깅하는것으로...
			if (!bGQCSRet && pOverlapped)
				break;

			if (&pSession->recvOverlapped == pOverlapped)
				pGameServer->RecvProc(pSession, dwNOBT);
			else
				pGameServer->SendProc(pSession, dwNOBT);

		} while (0);

		if (InterlockedDecrement(&pSession->IoCnt) == 0)
			pGameServer->ReleaseSession(pSession);
	}
	return 0;
}

BOOL GameServer::RecvPost(Session* pSession)
{
	WSABUF wsa[2];
	wsa[0].buf = pSession->recvRB.GetWriteStartPtr();
	wsa[0].len = pSession->recvRB.DirectEnqueueSize();
	wsa[1].buf = pSession->recvRB.Buffer_;
	wsa[1].len = pSession->recvRB.GetFreeSize() - wsa[0].len;

	ZeroMemory(&pSession->recvOverlapped, sizeof(WSAOVERLAPPED));
	DWORD flags = 0;
	InterlockedIncrement(&pSession->IoCnt);
	int iRecvRet = WSARecv(pSession->sock, wsa, 2, nullptr, &flags, &(pSession->recvOverlapped), nullptr);
	if (iRecvRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
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

BOOL GameServer::SendPost(Session* pSession)
{
	// 현재 값을 TRUE로 바꾼다. 원래 TRUE엿다면 반환값이 TRUE일것이며 그렇다면 현재 SEND 진행중이기 때문에 그냥 빠저나간다
	// 이 조건문의 위치로 인하여 Out은 바뀌지 않을것임이 보장된다.
	// 하지만 SendPost 실행주체가 Send완료통지 스레드인 경우에는 in의 위치는 SendPacket으로 인해서 바뀔수가 있다.
	// iUseSize를 구하는 시점에서의 DirectDequeueSize의 값이 달라질수있다.
	if (InterlockedExchange((LONG*)&pSession->bSendingInProgress, TRUE) == TRUE)
		return TRUE;

	// SendPacket에서 in을 옮겨서 UseSize가 0보다 커진시점에서 Send완료통지가 도착해서 Out을 옮기고 플래그 해제 Recv완료통지 스레드가 먼저 SendPost에 도달해 플래그를 선점한경우 UseSize가 0이나온다.
	// 여기서 flag를 다시 FALSE로 바꾸어주지 않아서 멈춤발생
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
	InterlockedIncrement(&pSession->IoCnt);
	ZeroMemory(&(pSession->sendOverlapped), sizeof(WSAOVERLAPPED));
	int iSendRet = WSASend(pSession->sock, wsa, i, nullptr, 0, &(pSession->sendOverlapped), nullptr);
	if (iSendRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
			return TRUE;

		InterlockedExchange((LONG*)&pSession->bSendingInProgress, FALSE);
		InterlockedDecrement(&(pSession->IoCnt));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

void GameServer::ReleaseSession(Session* pSession)
{
	ReleaseSendFailPacket(pSession);
	closesocket(pSession->sock);
	InterlockedExchange((LONG*)&pSession->bUsing, FALSE);
	SHORT shIndex = (SHORT)(pSession - pSessionArr_);
	EnterCriticalSection(&stackLock_);
	DisconnectStack_.Push((void**)&shIndex);
	LeaveCriticalSection(&stackLock_);
}

void GameServer::RecvProc(Session* pSession, DWORD dwNumberOfBytesTransferred)
{
	NET_HEADER header;
	Packet* pPacket = Packet::Alloc();
	pSession->recvRB.MoveInPos(dwNumberOfBytesTransferred);
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
		OnRecv(pSession->pPlayer, pPacket);
		pPacket->Clear();
	}
	RecvPost(pSession);
	Packet::Free(pPacket);
}

void GameServer::SendProc(Session* pSession, DWORD dwNumberOfBytesTransferred)
{
	ClearPacket(pSession);
	InterlockedExchange((LONG*)&pSession->bSendingInProgress, FALSE);

	if (pSession->sendRB.GetUseSize() > 0)
		SendPost(pSession);
}

BOOL GameServer::Start(DWORD dwMaxSession)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, GetStdHandle(STD_OUTPUT_HANDLE));

	int retval;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp OK!");
	// NOCT에 0들어가면 논리프로세서 수만큼을 설정함
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
	if (hListenSock_ == INVALID_SOCKET)
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

	hIOCPWorkerThreadArr_ = new HANDLE[si.dwNumberOfProcessors];
	for (DWORD i = 0; i < si.dwNumberOfProcessors; ++i)
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
		InterlockedExchange((LONG*)&pSessionArr_[i].bUsing, FALSE);
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

void GameServer::SendPacket(ID id, Packet* pPacket)
{
	Session* pSession = pSessionArr_ + GET_SESSION_INDEX(id);
	if (pSession->id.ullId == id.ullId && pSession->bUsing)
	{
		NET_HEADER* pNetHeader = (NET_HEADER*)pPacket->pBuffer_;
		pNetHeader->byCode = (BYTE)0x89;
		pNetHeader->byLen = pPacket->GetUsedDataSize() - 1;
		AcquireSRWLockExclusive(&pSession->sendRBLock);
		pSession->sendRB.Enqueue((const char*)&pPacket, sizeof(pPacket));
		ReleaseSRWLockExclusive(&pSession->sendRBLock);
		SendPost(pSession);
	}
}

BOOL GameServer::OnConnectionRequest()
{
	if (lSessionNum_ + 1 >= lMaxSession_)
		return FALSE;

	return TRUE;
}

void* GameServer::OnAccept(ID id)
{
	Pos playerPos;
	playerPos.shY = (rand() % (dfRANGE_MOVE_BOTTOM - 1)) + 1;
	playerPos.shX = (rand() % (dfRANGE_MOVE_RIGHT - 1)) + 1;

	SectorPos sector = CalcSector(playerPos);
	Player* pPlayer = ClientFreeList.Alloc();
	pPlayer->Init(id, playerPos);

	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, sector);
	do
	{
		AcquireSRWLockExclusive(&g_srwPlayerArrLock);
		AcquireSRWLockExclusive(&pPlayer->playerLock);

		//if (TryAcquireSectorAroundExclusive(&sectorAround))
		//	break;

		if (TryAcquireCreateDeleteSectorLock(&sectorAround, sector))
			break;

		ReleaseSRWLockExclusive(&pPlayer->playerLock);
		ReleaseSRWLockExclusive(&g_srwPlayerArrLock);
	} while (true);

	Packet* pSC_CREATE_MY_CHARACTER = Packet::Alloc();
	MAKE_SC_CREATE_MY_CHARACTER(pPlayer->dwID, MOVE_DIR_LL, playerPos, INIT_HP, pSC_CREATE_MY_CHARACTER);
	SendPacket(pPlayer->SessionId, pSC_CREATE_MY_CHARACTER);

	for (int i = 0; i < sectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			// 기존 플레이어의 존재를 알릴 패킷을 담을 직렬화 버퍼를 할당하고 패킷을 담아서 새로운 플레이어에게 전송
			Packet* pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pPlayer->dwID, MOVE_DIR_LL, playerPos, INIT_HP, pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);

			Player* pAncient = LinkToClient(pLink);
			AcquireSRWLockShared(&pAncient->playerLock);
			SendPacket(pAncient->SessionId,pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);

			// 기존 플레이어의 존재를 알릴 패킷을 담을 직렬화 버퍼를 할당하고 패킷을 담아서 새로운 플레이어에게 전송
			Packet* pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pAncient->dwID, pAncient->viewDir, pAncient->pos, pAncient->hp, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);
			SendPacket(id, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);

			// 만약 해당 기존 플레이어가 움직이는 중이라면 직렬화 버퍼를 할당해서 패킷을 담아서 새로운 플레이어에게 전송
			if (pAncient->moveDir != MOVE_DIR_NOMOVE)
			{
				Packet* pSC_MOVE_START_ANCIENT_TO_NEW = Packet::Alloc();
				MAKE_SC_MOVE_START(pAncient->dwID, pAncient->moveDir, pAncient->pos, pSC_MOVE_START_ANCIENT_TO_NEW);
				SendPacket(id, pSC_MOVE_START_ANCIENT_TO_NEW);
			}
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pAncient->playerLock);
		}
	}
	// 새로운 플레이어를 섹터에 추가한다
	AddClientAtSector(pPlayer, sector);

	// Update에서 사용할 배열에 Player를 추가한다.
	RegisterPlayer(pPlayer);
	//ReleaseSectorAroundExclusive(&sectorAround);
	ReleaseCreateDeleteSectorLock(&sectorAround, sector);
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
	ReleaseSRWLockExclusive(&g_srwPlayerArrLock);
	return pPlayer;
}

void GameServer::OnRelease(void* pPlayer)
{
	Player* pRlsPlayer = (Player*)pPlayer;

	SectorPos lastSector = CalcSector(pRlsPlayer->pos);
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, lastSector);
	do
	{
		AcquireSRWLockExclusive(&g_srwPlayerArrLock);
		AcquireSRWLockExclusive(&pRlsPlayer->playerLock);

		//if (TryAcquireSectorAroundExclusive(&sectorAround))
		//	break;
		if (TryAcquireCreateDeleteSectorLock(&sectorAround, lastSector))
			break;

		ReleaseSRWLockExclusive(&pRlsPlayer->playerLock);
		ReleaseSRWLockExclusive(&g_srwPlayerArrLock);
	} while (true);

	RemoveClientAtSector(pRlsPlayer, lastSector);
	for (int i = 0; i < sectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Packet* pSC_DELETE_CHARACTER_RELEASE_TO_ALL = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pRlsPlayer->dwID, pSC_DELETE_CHARACTER_RELEASE_TO_ALL);

			Player* pAncient = LinkToClient(pLink);
			AcquireSRWLockShared(&pAncient->playerLock);
			SendPacket(pAncient->SessionId, pSC_DELETE_CHARACTER_RELEASE_TO_ALL);
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pAncient->playerLock);
		}
	}
	//ReleaseSectorAroundExclusive(&sectorAround);
	ReleaseCreateDeleteSectorLock(&sectorAround, lastSector);
	ReleasePlayer(pRlsPlayer);
	ReleaseSRWLockExclusive(&pRlsPlayer->playerLock);
	ClientFreeList.Free(pRlsPlayer);
	ReleaseSRWLockExclusive(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ONRELEASE, EXCLUSIVE, RELEASE);
}

BOOL PacketProc(void* pClient, PACKET_TYPE packetType, Packet* pPacket);

void GameServer::OnRecv(void* pClient, Packet* pPacket)
{
	PACKET_TYPE packetType;
	(*pPacket) >> (*(BYTE*)(&packetType));
	PacketProc(pClient, packetType, pPacket);
}

BOOL PacketProc(void* pClient, PACKET_TYPE packetType, Packet* pPacket)
{
	switch (packetType)
	{
	case dfPACKET_CS_MOVE_START:
		return CS_MOVE_START_RECV((Player*)pClient, pPacket);
	case dfPACKET_CS_MOVE_STOP:
		return CS_MOVE_STOP_RECV((Player*)pClient, pPacket);
	case dfPACKET_CS_ATTACK1:
		return CS_ATTACK1_RECV((Player*)pClient, pPacket);
	case dfPACKET_CS_ATTACK2:
		return CS_ATTACK2_RECV((Player*)pClient, pPacket);
	case dfPACKET_CS_ATTACK3:
		return CS_ATTACK3_RECV((Player*)pClient, pPacket);
	case dfPACKET_CS_ECHO:
		return CS_ECHO_RECV((Player*)pClient, pPacket);
	default:
		return FALSE;
	}
}
