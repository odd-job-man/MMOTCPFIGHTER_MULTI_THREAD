#include <WinSock2.h>
#include <windows.h>
#include "Stack.h"
#include "GameServer.h"
#include "FreeList.h"
#include "Client.h"
#include "Constant.h"
#include "Sector.h"
#include "SCCContents.h"
#include "CSCContents.h"

FreeList<st_Client> ClientFreeList{ false,0 };
extern GameServer g_GameServer;

void* GameServer::OnAccept(ID id)
{
	Pos clientPos;
	clientPos.shY = 300;
	clientPos.shX = 300;

	SectorPos sector = CalcSector(clientPos);

	st_Client* pClient = ClientFreeList.Alloc();

	AcquireSRWLockExclusive(&pClient->clientLock);
	pClient->Init(id, clientPos, sector);

	Packet* pSC_CREATE_MY_CHARACTER = Packet::Alloc();
	MAKE_SC_CREATE_MY_CHARACTER(pClient->dwID, MOVE_DIR_LL, clientPos, INIT_HP, pSC_CREATE_MY_CHARACTER);
	SendPacket(pClient->SessionId, pSC_CREATE_MY_CHARACTER);

	// 주위 9개의 섹터에 락을건다
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, sector);
	AcquireSectorAroundExclusive(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			// 기존 플레이어의 존재를 알릴 패킷을 담을 직렬화 버퍼를 할당하고 패킷을 담아서 새로운 플레이어에게 전송
			Packet* pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pClient->dwID, MOVE_DIR_LL, clientPos, INIT_HP, pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);

			st_Client* pAncient = LinkToClient(pLink);
			AcquireSRWLockShared(&pAncient->clientLock);
			SendPacket(pAncient->SessionId,pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);

			// 기존 플레이어의 존재를 알릴 패킷을 담을 직렬화 버퍼를 할당하고 패킷을 담아서 새로운 플레이어에게 전송
			Packet* pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pAncient->dwID, pAncient->viewDir, pAncient->pos, pAncient->chHp, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);
			SendPacket(id, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);

			// 만약 해당 기존 플레이어가 움직이는 중이라면 직렬화 버퍼를 할당해서 패킷을 담아서 새로운 플레이어에게 전송
			if (pAncient->moveDir != MOVE_DIR_NOMOVE)
			{
				Packet* pSC_MOVE_START_ANCIENT_TO_NEW = Packet::Alloc();
				MAKE_SC_MOVE_START(pAncient->dwID, pAncient->moveDir, pAncient->pos, pSC_MOVE_START_ANCIENT_TO_NEW);
				SendPacket(id, pSC_MOVE_START_ANCIENT_TO_NEW);
			}
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pAncient->clientLock);
		}
	}
	// 새로운 플레이어를 섹터에 추가한다
	AddClientAtSector(pClient, sector);
	ReleaseSectorAroundExclusive(&sectorAround);
	ReleaseSRWLockExclusive(&pClient->clientLock);
	return pClient;
}

void GameServer::OnRelease(void* pClient)
{
	st_Client* pReleaseClient = (st_Client*)pClient;
	AcquireSRWLockExclusive(&pReleaseClient->clientLock);

	SectorPos curSector = CalcSector(pReleaseClient->pos);
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, curSector);
	AcquireSectorAroundExclusive(&sectorAround);
	RemoveClientAtSector(pReleaseClient, curSector);

	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			Packet* pSC_DELETE_CHARACTER_RELEASE_TO_ALL = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pReleaseClient->dwID, pSC_DELETE_CHARACTER_RELEASE_TO_ALL);

			st_Client* pAncient = LinkToClient(pLink);
			AcquireSRWLockShared(&pAncient->clientLock);
			SendPacket(pAncient->SessionId, pSC_DELETE_CHARACTER_RELEASE_TO_ALL);
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pAncient->clientLock);
		}
	}

	ReleaseSectorAroundExclusive(&sectorAround);
	ClientFreeList.Free(pReleaseClient);
	ReleaseSRWLockExclusive(&pReleaseClient->clientLock);
}

BOOL PacketProc(void* pClient, PACKET_TYPE packetType, Packet* pPacket);

void GameServer::OnRecv(void* pClient, Packet* pPacket)
{
	PACKET_TYPE packetType;
	(*pPacket) >> (*(BYTE*)(&packetType));
	PacketProc(pClient, packetType, pPacket);
}


BOOL GameServer::IsNetworkStateInvalid(ID SessionId)
{
	Session* pSession = pSessionArr_ + GET_SESSION_INDEX(SessionId);
	return (pSession->id.ullId == SessionId.ullId && pSession->bUsing);
}

int GameServer::GetAllValidClient(void** ppOutClientArr)
{
	int iCnt = 0;
	for (DWORD i = 0; i < MAX_SESSION; ++i)
	{
		if (!pSessionArr_[i].bUsing)
			continue;

		ppOutClientArr[iCnt++] = pSessionArr_[i].pClient;
	}
	return iCnt;
}

void GameServer::Update()
{
	st_Client* pClientArr[MAX_SESSION];
	int iPlayerNum = GetAllValidClient((void**)&pClientArr);

	for (int i = 0; i < iPlayerNum; ++i)
	{
		st_Client* pClient = pClientArr[i];
		AcquireSRWLockExclusive(&pClient->clientLock);

		if (pClient->moveDir == MOVE_DIR_NOMOVE)
		{
			ReleaseSRWLockExclusive(&pClient->clientLock);
			continue;
		}

		Pos oldPos = pClient->pos;
		Pos newPos;
		newPos.shY = oldPos.shY + vArr[pClient->moveDir].shY * dfSPEED_PLAYER_Y;
		newPos.shX = oldPos.shX + vArr[pClient->moveDir].shX * dfSPEED_PLAYER_X;

		if (!IsValidPos(newPos))
		{
			ReleaseSRWLockExclusive(&pClient->clientLock);
			continue;
		}

		SectorPos oldSector = CalcSector(oldPos);
		SectorPos newSector = CalcSector(newPos);

		if (IsSameSector(oldSector, newSector))
		{
			ReleaseSRWLockExclusive(&pClient->clientLock);
			continue;
		}

		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		SectorUpdateAndNotify(pClient, sectorMoveDir, oldSector, newSector, TRUE);
		ReleaseSRWLockExclusive(&pClient->clientLock);
	}
}



BOOL PacketProc(void* pClient, PACKET_TYPE packetType, Packet* pPacket)
{
	switch (packetType)
	{
	case dfPACKET_CS_MOVE_START:
		return CS_MOVE_START_RECV((st_Client*)pClient, pPacket);
	case dfPACKET_CS_MOVE_STOP:
		return CS_MOVE_STOP_RECV((st_Client*)pClient, pPacket);
	//case dfPACKET_CS_ATTACK1:
	//	return CS_ATTACK1_RECV((st_Client*)pClient, pPacket);
	//case dfPACKET_CS_ATTACK2:
	//	return CS_ATTACK2_RECV((st_Client*)pClient, pPacket);
	//case dfPACKET_CS_ATTACK3:
	//	return CS_ATTACK3_RECV((st_Client*)pClient, pPacket);
	//case dfPACKET_CS_ECHO:
	//	return CS_ECHO_RECV((st_Client*)pClient, pPacket);
	default:
		__debugbreak();
		return FALSE;
	}
}
