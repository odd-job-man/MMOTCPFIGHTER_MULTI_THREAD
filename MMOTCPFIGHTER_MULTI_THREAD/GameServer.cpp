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

void* GameServer::OnAccept(ID id)
{
	Pos ClientPos;
	//ClientPos.shY = (rand() % (dfRANGE_MOVE_BOTTOM - 1)) + 1;
	//ClientPos.shX = (rand() % (dfRANGE_MOVE_RIGHT - 1)) + 1;
	ClientPos.shY = 300;
	ClientPos.shX = 300;

	SectorPos sector = CalcSector(ClientPos);

	st_Client* pClient = ClientFreeList.Alloc();
	InitializeSRWLock(&pClient->clientLock);
	pClient->SessionId = id;
	pClient->dwID = ServerIDToClientID(id);
	pClient->pos = ClientPos;
	pClient->viewDir = MOVE_DIR_LL;
	pClient->moveDir = MOVE_DIR_NOMOVE;
	pClient->chHp = INIT_HP;
	pClient->CurSector = sector;
	pClient->OldSector = sector;

	Packet* pMSCMCPacket = Packet::Alloc();
	MAKE_SC_CREATE_MY_CHARACTER(pClient->dwID, MOVE_DIR_LL, ClientPos, INIT_HP, pMSCMCPacket);
	SendPacket(pClient->SessionId, pMSCMCPacket);

	// 주위 9개의 섹터에 락을건다
	LockAroundSectorShared(sector);
	AroundInfo* pAroundInfo = GetAroundValidClient(sector, pClient);
	for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
	{
		// 새로 접속한 플레이어에 대한 패킷을 담을 직렬화 버퍼를 할당한다
		Packet* pNewToOtherCreated = Packet::Alloc();
		st_Client* pOther = pAroundInfo->CI.cArr[i];
		// 직렬화 버퍼에 패킷을 담아서 전송
		MAKE_SC_CREATE_OTHER_CHARACTER(pClient->dwID, MOVE_DIR_LL, ClientPos, INIT_HP, pNewToOtherCreated);
		SendPacket(pOther->SessionId, pNewToOtherCreated);

		// 기존 플레이어의 존재를 알릴 패킷을 담을 직렬화 버퍼를 할당하고 패킷을 담아서 새로운 플레이어에게 전송
		Packet* pOtherToNewCreated = Packet::Alloc();
		MAKE_SC_CREATE_OTHER_CHARACTER(pOther->dwID, pOther->viewDir, pOther->pos, pOther->chHp, pOtherToNewCreated);
		SendPacket(id, pOtherToNewCreated);
		// 만약 해당 기존 플레이어가 움직이는 중이라면 직렬화 버퍼를 할당해서 패킷을 담아서 새로운 플레이어에게 전송
		if (pOther->moveDir != MOVE_DIR_NOMOVE)
		{
			Packet* pOtherMoveStart = Packet::Alloc();
			MAKE_SC_MOVE_START(pOther->dwID, pOther->moveDir, pOther->pos, pOtherMoveStart);
			SendPacket(id, pOtherMoveStart);
		}
	}
	// 새로운 플레이어를 섹터에 추가한다
	AddClientAtSector(pClient, sector);
	// 주위 9개의 섹터에 락을 푼다
	UnLockAroundSectorShared(sector);
	return pClient;
}

BOOL PacketProc(void* pClient, BYTE byPacketType, Packet* pPacket);

void GameServer::OnRecv(void* pClient, Packet* pPacket)
{
	BYTE byPacketType;
	(*pPacket) >> byPacketType;
	PacketProc(pClient, byPacketType, pPacket);
}

BOOL PacketProc(void* pClient, BYTE byPacketType, Packet* pPacket)
{
	switch (byPacketType)
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
