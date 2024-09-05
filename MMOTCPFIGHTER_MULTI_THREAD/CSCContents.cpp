#include <windows.h>
#include "Direction.h"
#include "CSCContents.h"
#include "SCCContents.h"
#include "Sector.h"
#include "Stack.h"
#include "GameServer.h"
extern GameServer g_GameServer;

BOOL CS_MOVE_START(st_Client* pClient, MOVE_DIR moveDir, Pos clientPos)
{
	// Ŭ���̾�Ʈ �ü�ó��
	AcquireSRWLockExclusive(&pClient->clientLock);
	switch (moveDir)
	{
	case MOVE_DIR_RU:
	case MOVE_DIR_RR:
	case MOVE_DIR_RD:
		pClient->viewDir = MOVE_DIR_RR;
		break;
	case MOVE_DIR_LL:
	case MOVE_DIR_LU:
	case MOVE_DIR_LD:
		pClient->viewDir = MOVE_DIR_LL;
		break;
	}
	ReleaseSRWLockExclusive(&pClient->clientLock);

	AcquireSRWLockShared(&pClient->clientLock);
	SectorPos oldSector = CalcSector(pClient->pos);

	// �̵��߿� ������ �ٲ㼭 STOP�� �����ʰ� �� �ٽ� START�� �Ӵµ�, �̶� ���� �������� �������� ��ũ�� �߻��ϴ� ���.
	// ���� ���� ��ǥ �������� �ٹ� ���Ϳ� SYNC�޽����� �������ϸ� ���� �� ������ǥ �ٹ漽�Ϳ� ���� ������´�
	if (IsSync(pClient->pos, clientPos))
	{
		LockAroundSectorShared(oldSector);
		AroundInfo* pAroundInfo = GetAroundValidClient(oldSector, nullptr);
		for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
		{
			Packet* pSyncPacket = Packet::Alloc();
			g_GameServer.SendPacket(pClient->SessionId, pSyncPacket);
		}
		clientPos = pClient->pos;
		UnLockAroundSectorShared(oldSector);
	}

	SectorPos newSector = CalcSector(pClient->pos);

	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ������ �� �����δٰ� �����ű⿡ ������ �����δٰ� �˸��ʿ� ����.
		SectorUpdateAndNotify(pClient, bySectorMoveDir, oldSector, newSector, FALSE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	// �ٹ漽�Ϳ� �÷��̾ �߰� Ȥ�� �����Ǵ°��� �ƴϱ⿡ 9�� ���Ϳ� SHARED�� ���� �Ǵ�
	LockAroundSectorShared(newSector);
	AroundInfo* pAroundInfo = GetAroundValidClient(newSector, pClient);

	// ���� �ƹ��������� �׳� Ŭ����¸� �ݿ��ϰ� ��Ǯ�� ��
	if (pAroundInfo->CI.dwNum <= 0)
	{
		pClient->moveDir = moveDir;
		pClient->pos = clientPos;
		UnLockAroundSectorShared(newSector);
		return TRUE;
	}

	// ������ �ִٸ� �ܾ�� ��ȿ�� �÷��̾� ��Ͽ� ���� MOVE_START ��Ŷ�� �� �� ��Ǯ�� ����
	Packet* pMoveStartPacket = Packet::Alloc();
	for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
	{
		MAKE_SC_MOVE_START(dwFromId, moveDir, clientPos, pMoveStartPacket);
		g_GameServer.SendPacket(pClient->SessionId, pMoveStartPacket);
	}
	UnLockAroundSectorShared(newSector);
	ReleaseSRWLockShared(&pClient->clientLock);
    return TRUE;
}
