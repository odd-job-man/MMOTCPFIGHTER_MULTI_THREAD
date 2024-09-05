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

	// ���� 9���� ���Ϳ� �����Ǵ�
	LockAroundSectorShared(sector);
	AroundInfo* pAroundInfo = GetAroundValidClient(sector, pClient);
	for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
	{
		// ���� ������ �÷��̾ ���� ��Ŷ�� ���� ����ȭ ���۸� �Ҵ��Ѵ�
		Packet* pNewToOtherCreated = Packet::Alloc();
		st_Client* pOther = pAroundInfo->CI.cArr[i];
		// ����ȭ ���ۿ� ��Ŷ�� ��Ƽ� ����
		MAKE_SC_CREATE_OTHER_CHARACTER(pClient->dwID, MOVE_DIR_LL, ClientPos, INIT_HP, pNewToOtherCreated);
		SendPacket(pOther->SessionId, pNewToOtherCreated);

		// ���� �÷��̾��� ���縦 �˸� ��Ŷ�� ���� ����ȭ ���۸� �Ҵ��ϰ� ��Ŷ�� ��Ƽ� ���ο� �÷��̾�� ����
		Packet* pOtherToNewCreated = Packet::Alloc();
		MAKE_SC_CREATE_OTHER_CHARACTER(pOther->dwID, pOther->viewDir, pOther->pos, pOther->chHp, pOtherToNewCreated);
		SendPacket(id, pOtherToNewCreated);
		// ���� �ش� ���� �÷��̾ �����̴� ���̶�� ����ȭ ���۸� �Ҵ��ؼ� ��Ŷ�� ��Ƽ� ���ο� �÷��̾�� ����
		if (pOther->moveDir != MOVE_DIR_NOMOVE)
		{
			Packet* pOtherMoveStart = Packet::Alloc();
			MAKE_SC_MOVE_START(pOther->dwID, pOther->moveDir, pOther->pos, pOtherMoveStart);
			SendPacket(id, pOtherMoveStart);
		}
	}
	// ���ο� �÷��̾ ���Ϳ� �߰��Ѵ�
	AddClientAtSector(pClient, sector);
	// ���� 9���� ���Ϳ� ���� Ǭ��
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
