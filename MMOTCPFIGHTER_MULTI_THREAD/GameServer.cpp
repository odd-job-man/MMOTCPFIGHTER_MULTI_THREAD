#include "GameServer.h"
#include "FreeList.h"
#include "Client.h"
#include "Constant.h"
#include "Sector.h"
#include "SCCContents.h"

FreeList<st_Client> ClientFreeList{ false,0 };

void* GameServer::OnAccept(ID id)
{
    Pos ClientPos;
	ClientPos.shY = (rand() % (dfRANGE_MOVE_BOTTOM - 1)) + 1;
	ClientPos.shX = (rand() % (dfRANGE_MOVE_RIGHT - 1)) + 1;

	SectorPos sector;
	CalcSector(&sector, ClientPos);

	st_Client* pClient = ClientFreeList.Alloc();
	pClient->id = id;
	pClient->pos = ClientPos;
	pClient->byViewDir = dfPACKET_MOVE_DIR_LL;
	pClient->byMoveDir = dfPACKET_MOVE_DIR_NOMOVE;
	pClient->chHp = INIT_HP;
	pClient->CurSector = sector;

	Packet* pMSCMCPacket = Packet::Alloc();
	MAKE_SC_CREATE_MY_CHARACTER(pClient->id, dfPACKET_MOVE_DIR_LL, ClientPos, INIT_HP, pMSCMCPacket);
	SendPacket(pClient->id, pMSCMCPacket);

	// 주위 9개의 섹터에 락을건다
	LockAroundSector(sector);
	AroundInfo* pAroundInfo = GetAroundValidClient(sector, pClient);
	for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
	{
		// 새로 접속한 플레이어에 대한 패킷을 담을 직렬화 버퍼를 할당한다
		Packet* pNewToOtherCreated = Packet::Alloc();
		st_Client* pOther = pAroundInfo->CI.cArr[i];
		// 직렬화 버퍼에 패킷을 담아서 전송
		MAKE_SC_CREATE_OTHER_CHARACTER(id, dfPACKET_MOVE_DIR_LL, ClientPos, INIT_HP, pNewToOtherCreated);
		SendPacket(pOther->id, pNewToOtherCreated);

		// 기존 플레이어의 존재를 알릴 패킷을 담을 직렬화 버퍼를 할당하고 패킷을 담아서 새로운 플레이어에게 전송
		Packet* pOtherToNewCreated = Packet::Alloc();
		MAKE_SC_CREATE_OTHER_CHARACTER(pOther->id, pOther->byViewDir, pOther->pos, pOther->chHp, pOtherToNewCreated);
		SendPacket(id, pOtherToNewCreated);
		// 만약 해당 기존 플레이어가 움직이는 중이라면 직렬화 버퍼를 할당해서 패킷을 담아서 새로운 플레이어에게 전송
		if (pOther->byMoveDir != dfPACKET_MOVE_DIR_NOMOVE)
		{
			Packet* pOtherMoveStart = Packet::Alloc();
			MAKE_SC_MOVE_START(pOther->id, pOther->byMoveDir, pOther->pos, pOtherMoveStart);
			SendPacket(id, pOtherMoveStart);
		}
	}
	// 새로운 플레이어를 섹터에 추가한다
	AddClientAtSector(pClient, sector);
	// 주위 9개의 섹터에 락을 푼다
	UnLockAroundSector(sector);
	return pClient;
}
