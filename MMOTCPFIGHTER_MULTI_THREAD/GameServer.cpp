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

	// ���� 9���� ���Ϳ� �����Ǵ�
	LockAroundSector(sector);
	AroundInfo* pAroundInfo = GetAroundValidClient(sector, pClient);
	for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
	{
		// ���� ������ �÷��̾ ���� ��Ŷ�� ���� ����ȭ ���۸� �Ҵ��Ѵ�
		Packet* pNewToOtherCreated = Packet::Alloc();
		st_Client* pOther = pAroundInfo->CI.cArr[i];
		// ����ȭ ���ۿ� ��Ŷ�� ��Ƽ� ����
		MAKE_SC_CREATE_OTHER_CHARACTER(id, dfPACKET_MOVE_DIR_LL, ClientPos, INIT_HP, pNewToOtherCreated);
		SendPacket(pOther->id, pNewToOtherCreated);

		// ���� �÷��̾��� ���縦 �˸� ��Ŷ�� ���� ����ȭ ���۸� �Ҵ��ϰ� ��Ŷ�� ��Ƽ� ���ο� �÷��̾�� ����
		Packet* pOtherToNewCreated = Packet::Alloc();
		MAKE_SC_CREATE_OTHER_CHARACTER(pOther->id, pOther->byViewDir, pOther->pos, pOther->chHp, pOtherToNewCreated);
		SendPacket(id, pOtherToNewCreated);
		// ���� �ش� ���� �÷��̾ �����̴� ���̶�� ����ȭ ���۸� �Ҵ��ؼ� ��Ŷ�� ��Ƽ� ���ο� �÷��̾�� ����
		if (pOther->byMoveDir != dfPACKET_MOVE_DIR_NOMOVE)
		{
			Packet* pOtherMoveStart = Packet::Alloc();
			MAKE_SC_MOVE_START(pOther->id, pOther->byMoveDir, pOther->pos, pOtherMoveStart);
			SendPacket(id, pOtherMoveStart);
		}
	}
	// ���ο� �÷��̾ ���Ϳ� �߰��Ѵ�
	AddClientAtSector(pClient, sector);
	// ���� 9���� ���Ϳ� ���� Ǭ��
	UnLockAroundSector(sector);
	return pClient;
}
