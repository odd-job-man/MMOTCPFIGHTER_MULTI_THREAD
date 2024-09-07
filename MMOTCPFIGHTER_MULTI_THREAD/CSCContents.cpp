#include <WinSock2.h>
#include <windows.h>
#include "Direction.h"
#include "CSCContents.h"
#include "SCCContents.h"
#include "Sector.h"
#include "Stack.h"
#include "GameServer.h"
extern GameServer g_GameServer;

__forceinline BOOL IsRRColide(Pos AttackerPos, Pos VictimPos, SHORT shAttackRangeY, SHORT shAttackRangeX)
{
	BOOL IsYCollide;
	BOOL IsXCollide;
	IsYCollide = ((AttackerPos.shY - shAttackRangeY / 2) < VictimPos.shY) && (VictimPos.shY < (AttackerPos.shY + shAttackRangeY / 2));
	IsXCollide = (AttackerPos.shX < VictimPos.shX) && (AttackerPos.shX + shAttackRangeX) > VictimPos.shX;
	return IsXCollide && IsYCollide;
}

__forceinline BOOL IsLLColide(Pos AttackerPos, Pos VictimPos, SHORT shAttackRangeY, SHORT shAttackRangeX)
{
	BOOL IsYCollide;
	BOOL IsXCollide;
	IsYCollide = ((AttackerPos.shY - shAttackRangeY / 2) < VictimPos.shY) && (VictimPos.shY < (AttackerPos.shY + shAttackRangeY / 2));
	IsXCollide = (AttackerPos.shX > VictimPos.shX) && (AttackerPos.shX - shAttackRangeX) < VictimPos.shX;
	return IsXCollide && IsYCollide;
}

__forceinline void SyncProc(st_Client* pSyncClient, SectorPos recvSector)
{
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, recvSector);
	LockShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pSyncPacket = Packet::Alloc();
			MAKE_SC_SYNC(pSyncClient->dwID, pSyncClient->pos, pSyncPacket);
			g_GameServer.SendPacket(pOtherClient->SessionId, pSyncPacket);
			pLink = pLink->pNext;
		}
	}
	UnLockShared(&sectorAround);
}

void FindSectorInAttackRange(Pos pos, MOVE_DIR viewDir, SHORT shAttackRangeY, SHORT shAttackRangeX, st_SECTOR_AROUND* pSectorAround)
{
	SectorPos originalSector = CalcSector(pos);
	SectorPos temp;

	pSectorAround->Around[0].YX = originalSector.YX;
	int iCount = 1;

	if (viewDir == MOVE_DIR_LL)
	{
		// ���� ���� ����
		// temp.shX ���� ���͵��� X�� ���� X��.

		temp.shX = (pos.shX - shAttackRangeX) / df_SECTOR_WIDTH;
		// �ٷο��� ���� ��ȿ�� �����Ӱ� ���ÿ� ���ݹ�����
		if (IsValidSector(originalSector.shY, temp.shX) && originalSector.shX != temp.shX)
		{
			pSectorAround->Around[iCount].shX = temp.shX;
			pSectorAround->Around[iCount].shY = originalSector.shY;
			++iCount;

			// �̽������� ������ ��ȿ�ϸ� ��, �Ʒ� Ȯ���ؾ���
			temp.shY = (pos.shY + shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				// ��
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// ���� ��
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}

			// �Ʒ� Ȯ��
			temp.shY = (pos.shY - shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				//�Ʒ�
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// ���� �Ʒ�
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}
		}
		else
		{
			// ���ʹ����� �ٸ����ͷ� �Ѿ�� ������ �� �Ʒ��� Ž���ϸ��
			temp.shY = (pos.shY + shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}


			temp.shY = (pos.shY - shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}
		}
	}
	else
	{
		// ������ ���� ����
		// ������ ���� ���Ͱ˻�
		temp.shX = (pos.shX + shAttackRangeX) / df_SECTOR_WIDTH;
		if (IsValidSector(originalSector.shY, temp.shX) && originalSector.shX != temp.shX)
		{
			// ������ Ȯ�� �� �Ʒ� �˻�
			pSectorAround->Around[iCount].shX = temp.shX;
			pSectorAround->Around[iCount].shY = originalSector.shY;
			++iCount;

			// �̽������� �������� ��ȿ�ϸ� ��, �Ʒ� Ȯ���ؾ���
			temp.shY = (pos.shY + shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				// ��
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// ���� ��
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}

			// �Ʒ� Ȯ��
			temp.shY = (pos.shY - shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				//�Ʒ�
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// ���� �Ʒ�
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}

		}
		else
		{
			// �����ʹ����� �ٸ����ͷ� �Ѿ�� ������ �� �Ʒ��� Ž���ϸ��
			temp.shY = (pos.shY + shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}


			temp.shY = (pos.shY - shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}
		}
	}
	pSectorAround->byCount = iCount;
}

st_Client* HandleCollision(st_Client* pAttacker, SHORT shAttackRangeY, SHORT shAttackRangeX, st_SECTOR_AROUND* pSectorAround)
{
	st_Client* pVictim = nullptr;

	if (pAttacker->viewDir == MOVE_DIR_LL)
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
			{
				st_Client* pOtherClient = LinkToClient(pLink);

				// ������ ������ ���� �������Ѵ�
				if (pOtherClient == pAttacker)
					continue;

				AcquireSRWLockShared(&pOtherClient->clientLock);
				if (IsLLColide(pAttacker->pos, pOtherClient->pos, shAttackRangeY, shAttackRangeX))
				{
					pVictim = pOtherClient;
					ReleaseSRWLockShared(&pOtherClient->clientLock);
					return pVictim;
				}
				ReleaseSRWLockShared(&pOtherClient->clientLock);
				pLink = pLink->pNext;
			}
		}
	}
	else
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
			{
				st_Client* pOtherClient = LinkToClient(pLink);

				// ������ ������ ���� �������Ѵ�
				if (pOtherClient == pAttacker)
					continue;

				AcquireSRWLockShared(&pOtherClient->clientLock);
				if (IsRRColide(pAttacker->pos, pOtherClient->pos, shAttackRangeY, shAttackRangeX))
				{
					pVictim = pOtherClient;
					ReleaseSRWLockShared(&pOtherClient->clientLock);
					return pVictim;
				}
				ReleaseSRWLockShared(&pOtherClient->clientLock);
				pLink = pLink->pNext;
			}
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(st_Client* pClient, MOVE_DIR moveDir, Pos clientPos)
{
	AcquireSRWLockExclusive(&pClient->clientLock);
	SectorPos oldSector = CalcSector(pClient->pos);

	// Ŭ���̾�Ʈ �ü�ó��
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

	// �̵��߿� ������ �ٲ㼭 STOP�� �����ʰ� �� �ٽ� START�� �Ӵµ�, �̶� ���� �������� �������� ��ũ�� �߻��ϴ� ���.
	// ���� ���� ��ǥ �������� �ٹ� ���Ϳ� SYNC�޽����� �������ϸ� ���� �� ������ǥ �ٹ漽�Ϳ� ���� ������´�
	if (IsSync(pClient->pos, clientPos))
	{
		SyncProc(pClient, oldSector);
		clientPos = pClient->pos;
	}
	else
	{
		// ��ũ�� �ƴϹǷ� �������� Ŭ�� ��ǥ�� �ϴ´�
		pClient->pos = clientPos;
	}


	SectorPos newSector = CalcSector(clientPos);

	// IsSync�� ���̾��ٸ� �̽������� oldSector == newSector�� if�� �Ȱ�ħ
	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ������ �� �����δٰ� �����ű⿡ ������ �����δٰ� �˸��ʿ� ����.
		SectorUpdateAndNotify(pClient, bySectorMoveDir, oldSector, newSector, FALSE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	// �ٹ漽�Ϳ� �÷��̾ �߰� Ȥ�� �����Ǵ°��� �ƴϱ⿡ 9�� ���Ϳ� SHARED�� ���� �Ǵ�
	// ���� SC_MOVE_START ��Ŷ �� �� ��Ǯ�� ����
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	LockShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pMoveStartPacket = Packet::Alloc();
			MAKE_SC_MOVE_START(pClient->dwID, pClient->moveDir, clientPos, pMoveStartPacket);
			g_GameServer.SendPacket(pClient->SessionId, pMoveStartPacket);
			pLink = pLink->pNext;
		}
	}
	UnLockShared(&sectorAround);
	ReleaseSRWLockExclusive(&pClient->clientLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(st_Client* pClient, MOVE_DIR viewDir, Pos clientPos)
{
	AcquireSRWLockExclusive(&pClient->clientLock);
	SectorPos oldSector = CalcSector(pClient->pos);

	// START���� �ð��� �귯 Stop�� �Ӵµ�, �� ���̿� ���� �������� �������� ��ũ�� �߻��ϴ� ���.
	if (IsSync(pClient->pos, clientPos))
	{
		SyncProc(pClient, oldSector);
		clientPos = pClient->pos;
	}
	else
	{
		// ��ũ�� �ƴϹǷ� �������� Ŭ�� ��ǥ�� �ϴ´�
		pClient->pos = clientPos;
	}

	SectorPos newSector = CalcSector(clientPos);

	// IsSync�� ���̾��ٸ� �̽������� oldSector == newSector�� if�� �Ȱ�ħ
	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ���� �����̱� ������ ���� ���̴� �ֵ����� �����δٰ� ������ �Ʒ����� �ٽ� Stop ��Ŷ�� �����°� ���̵ȴ�
		SectorUpdateAndNotify(pClient, bySectorMoveDir, oldSector, newSector, TRUE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	// �ٹ漽�Ϳ� �÷��̾ �߰� Ȥ�� �����Ǵ°��� �ƴϱ⿡ 9�� ���Ϳ� SHARED�� ���� �Ǵ�
	// ���� SC_MOVE_STOP ��Ŷ �� �� ��Ǯ�� ����
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	LockShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pMoveStopPacket = Packet::Alloc();
			MAKE_SC_MOVE_STOP(pClient->dwID, pClient->viewDir, clientPos, pMoveStopPacket);
			g_GameServer.SendPacket(pClient->SessionId, pMoveStopPacket);
			pLink = pLink->pNext;
		}
	}
	UnLockShared(&sectorAround);
	ReleaseSRWLockExclusive(&pClient->clientLock);
    return TRUE;
}

__forceinline void DamageProc(st_Client* pAttacker, st_Client* pVictim, st_SECTOR_AROUND* pSectorAround)
{
	for (BYTE i = 0; i < pSectorAround->byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pDamagePacket = Packet::Alloc();
			MAKE_SC_DAMAGE(pAttacker->dwID, pVictim->dwID, pVictim->chHp, pDamagePacket);

			AcquireSRWLockShared(&pOtherClient->clientLock);
			g_GameServer.SendPacket(pOtherClient->SessionId, pDamagePacket);
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pOtherClient->clientLock);
		}
	}
}

BOOL CS_ATTACK1(st_Client* pClient, MOVE_DIR viewDir, Pos clientPos)
{
	AcquireSRWLockShared(&pClient->clientLock);

	SectorPos curSector = CalcSector(clientPos);
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, curSector);
	LockShared(&sectorAround);

	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pAttack1Packet = Packet::Alloc();

			// ������ ������ ���� �������Ѵ�
			if (pOtherClient == pClient)
				continue;

			MAKE_SC_ATTACK1(pClient->dwID, pClient->viewDir, pClient->pos, pAttack1Packet);
			AcquireSRWLockShared(&pOtherClient->clientLock);
			g_GameServer.SendPacket(pOtherClient->SessionId, pAttack1Packet);
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pOtherClient->clientLock);
		}
	}

	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(clientPos, viewDir, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision(pClient, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);

	if (!pVictim)
	{
		UnLockShared(&sectorAround);
		ReleaseSRWLockShared(&pClient->clientLock);
		return FALSE;
	}

	pVictim->chHp -= dfATTACK1_DAMAGE;
	DamageProc(pClient, pVictim, &sectorAround);

	UnLockShared(&sectorAround);
	ReleaseSRWLockShared(&pClient->clientLock);
	return TRUE;
}

BOOL CS_ATTACK2(st_Client* pClient, MOVE_DIR viewDir, Pos ClientPos)
{
	AcquireSRWLockShared(&pClient->clientLock);
	ReleaseSRWLockShared(&pClient->clientLock);
	return TRUE;
}

