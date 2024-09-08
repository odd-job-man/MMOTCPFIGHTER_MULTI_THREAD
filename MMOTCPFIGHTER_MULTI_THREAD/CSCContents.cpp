#include <WinSock2.h>
#include <windows.h>
#include "Session.h"
#include "Direction.h"
#include "CSCContents.h"
#include "SCCContents.h"
#include "Sector.h"
#include "Stack.h"
#include "IHandler.h"
#include "GameServer.h"
#include <stdio.h>
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

__forceinline void SyncProc(st_Client* pSyncPlayer, SectorPos recvSector)
{
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, recvSector);
	AcquireSectorAroundShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			// ���� �������� ���� �ű��մ� ���ֹ� and ��ũ ����ڿ��Ե� �������ϸ� �� �������� SessionId�� �ʿ��ؼ� ���� ���� �ʾ���
			st_Client* pAncient = LinkToClient(pLink);
			Packet* pSyncPacket = Packet::Alloc();
			MAKE_SC_SYNC(pSyncPlayer->dwID, pSyncPlayer->pos, pSyncPacket);
			g_GameServer.SendPacket(pAncient->SessionId, pSyncPacket);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
}

// �����̱� ������ �÷��̾� �ü�ó��
__forceinline void ProcessPlayerViewDir(st_Client* pMoveStartPlayer, MOVE_DIR moveDir)
{
	switch (moveDir)
	{
	case MOVE_DIR_RU:
	case MOVE_DIR_RR:
	case MOVE_DIR_RD:
		pMoveStartPlayer->viewDir = MOVE_DIR_RR;
		break;
	case MOVE_DIR_LL:
	case MOVE_DIR_LU:
	case MOVE_DIR_LD:
		pMoveStartPlayer->viewDir = MOVE_DIR_LL;
		break;
	}
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

// �ǰݽ� ���� Ǯ�� �ʴ� ������ �� ���̿� OnRelease�� ���ؼ� �������ų� Ȥ�� �׷����� �ٸ� Ŭ��� �ٲ�°��� ���� �����̴�.
st_Client* HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(st_Client* pAttacker, SHORT shAttackRangeY, SHORT shAttackRangeX, st_SECTOR_AROUND* pSectorAround)
{
	if (pAttacker->viewDir == MOVE_DIR_LL)
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
			{
				st_Client* pHitCandidate = LinkToClient(pLink);

				if (pHitCandidate == pAttacker)
				{
					pLink = pLink->pNext;
					continue;
				}

				AcquireSRWLockExclusive(&pHitCandidate->playerLock);
				if (IsLLColide(pAttacker->pos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
					return pHitCandidate;

				pLink = pLink->pNext;
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
			}
		}
	}
	else
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
			{
				st_Client* pHitCandidate = LinkToClient(pLink);

				// ������ ������ ���� �������Ѵ�
				if (pHitCandidate == pAttacker)
				{
					pLink = pLink->pNext;
					continue;
				}

				AcquireSRWLockExclusive(&pHitCandidate->playerLock);
				if (IsRRColide(pAttacker->pos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
					return pHitCandidate;

				pLink = pLink->pNext;
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
			}
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(st_Client* pPlayer, MOVE_DIR moveDir, Pos clientPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	//printf("CS_MOVE_START! ThreadID : %u (%d, %d)\n", GetCurrentThreadId(), clientPos.shX, clientPos.shY);
	SectorPos oldSector = CalcSector(pPlayer->pos);

	// Ŭ���̾�Ʈ �ü�ó��
	ProcessPlayerViewDir(pPlayer, moveDir);
	pPlayer->moveDir = moveDir;

	// �̵��߿� ������ �ٲ㼭 STOP�� �����ʰ� �� �ٽ� START�� �Ӵµ�, �̶� ���� �������� �������� ��ũ�� �߻��ϴ� ���.
	// ���� ���� ��ǥ �������� �ٹ� ���Ϳ� SYNC�޽����� �������ϸ� ���� �� ������ǥ �ٹ漽�Ϳ� ���� ������´�
	if (IsSync(pPlayer->pos, clientPos))
	{
		//printf("Sync! ThreadID : %u\n", GetCurrentThreadId());
		SyncProc(pPlayer, oldSector);
		clientPos = pPlayer->pos;
	}
	else
	{
		// ��ũ�� �ƴϹǷ� �������� Ŭ�� ��ǥ�� �ϴ´�
		pPlayer->pos = clientPos;
	}


	SectorPos newSector = CalcSector(clientPos);

	// IsSync�� ���̾��ٸ� �̽������� oldSector == newSector�� if�� �Ȱ�ħ
	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		//printf("CS_MOVE_START! ThreadID : %u SectorMOVE (%d,%d) -> (%d,%d)\n", GetCurrentThreadId(), oldSector.shX, oldSector.shY, newSector.shX, newSector.shY);
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ������ �� �����δٰ� �����ű⿡ ������ �����δٰ� �˸��ʿ� ����.
		SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, FALSE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	// �ٹ漽�Ϳ� �÷��̾ �߰� Ȥ�� �����Ǵ°��� �ƴϱ⿡ 9�� ���Ϳ� SHARED�� ���� �Ǵ�
	// ���� SC_MOVE_START ��Ŷ �� �� ��Ǯ�� ����
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	AcquireSectorAroundShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAncient = LinkToClient(pLink);
			// �� �ڽſ��Դ� START�� �Ⱥ�������, �̰� if�� ��ġ������ �޸� �� ������
			if (pAncient == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			Packet* pSC_MOVE_START = Packet::Alloc();
			MAKE_SC_MOVE_START(pPlayer->dwID, pPlayer->moveDir, clientPos, pSC_MOVE_START);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_START);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	//printf("CS_MOVE_STOP! ThreadID : %u (%d, %d)\n", GetCurrentThreadId(), playerPos.shX, playerPos.shY);
	SectorPos oldSector = CalcSector(pPlayer->pos);
	pPlayer->moveDir = MOVE_DIR_NOMOVE;

	// START���� �ð��� �귯 Stop�� �Ӵµ�, �� ���̿� ���� �������� �������� ��ũ�� �߻��ϴ� ���.
	if (IsSync(pPlayer->pos, playerPos))
	{
		SyncProc(pPlayer, oldSector);
		playerPos = pPlayer->pos;
	}
	else
	{
		// ��ũ�� �ƴϹǷ� �������� Ŭ�� ��ǥ�� �ϴ´�
		pPlayer->pos = playerPos;
	}

	SectorPos newSector = CalcSector(playerPos);

	// IsSync�� ���̾��ٸ� �̽������� oldSector == newSector�� if�� �Ȱ�ħ
	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		//printf("CS_MOVE_STOP! ThreadID : %u SectorMOVE (%d,%d) -> (%d,%d)\n", GetCurrentThreadId(), oldSector.shX, oldSector.shY, newSector.shX, newSector.shY);
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ���� �����̱� ������ ���� ���̴� �ֵ����� �����δٰ� ������ �Ʒ����� �ٽ� Stop ��Ŷ�� �����°� ���̵ȴ�
		SectorUpdateAndNotify(pPlayer, bySectorMoveDir, oldSector, newSector, TRUE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	// �ٹ漽�Ϳ� �÷��̾ �߰� Ȥ�� �����Ǵ°��� �ƴϱ⿡ 9�� ���Ϳ� SHARED�� ���� �Ǵ�
	// ���� SC_MOVE_STOP ��Ŷ �� �� ��Ǯ�� ����
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	AcquireSectorAroundShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAncient = LinkToClient(pLink);
			// SC_MOVE_STOP�� ����ڸ� �����ϰ� �����ؾ��Ѵ�
			if (pAncient == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}
			// ���ֹ��� SessionId�� ����ϱ⿡ ���� ���� �ʾѴ�
			Packet* pSC_MOVE_STOP = Packet::Alloc();
			MAKE_SC_MOVE_STOP(pPlayer->dwID, pPlayer->viewDir, playerPos, pSC_MOVE_STOP);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_STOP);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
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
			st_Client* pDamageViewer = LinkToClient(pLink);
			Packet* pDamagePacket = Packet::Alloc();
			MAKE_SC_DAMAGE(pAttacker->dwID, pVictim->dwID, pVictim->chHp, pDamagePacket);

			// �̹� ���Ϳ� ���� �ɷ�������,x,y,hp,movedir,viewdir,���� ������ �ʿ� ���� ������ DamageView�� �������� ����
			g_GameServer.SendPacket(pDamageViewer->SessionId, pDamagePacket);
			pLink = pLink->pNext;
		}
	}
}

BOOL CS_ATTACK1(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);

	// �ǰ������� ������ ��Ŷ�� ���� ���͸� ���ؼ� ���� �Ǵ�
	st_SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// ���ݸ���� ���� SC_ATTACK�� �ֺ� 9���� ���Ϳ��� �ڽ��� ������ �÷��̾�鿡�� �������Ѵ�
	for (BYTE i = 0; i < attackSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAttackViewer = LinkToClient(pLink);

			// ������ ������ ���� �������Ѵ�
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			// �׳� AttackViewer�� SessionID�� �ʿ��ؼ� ���� ���� �ʾ���.
			Packet* pSC_ATTACK1 = Packet::Alloc();
			MAKE_SC_ATTACK1(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK1);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK1);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(pPlayer, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);

	if (!pVictim)
	{
		ReleaseSRWLockShared(&pPlayer->playerLock);
		return FALSE;
	}

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.

	// SC_DAMAGE ���� ���� ���ϱ�
	st_SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE���� ���Ͷ� SHARED�� ���� �Ǳ�� ��������Ŷ ������, ���Ͷ� Ǯ��, �ǰ��� Exclusive�� Ǯ��, ��ȯ
	AcquireSectorAroundShared(&damageSectorAround);
	pVictim->chHp -= dfATTACK1_DAMAGE;
	DamageProc(pPlayer, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	ReleaseSRWLockShared(&pPlayer->playerLock);
	return TRUE;
}

BOOL CS_ATTACK2(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);

	// �ǰ������� ������ ��Ŷ�� ���� ���͸� ���ؼ� ���� �Ǵ�
	st_SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// ���ݸ���� ���� SC_ATTACK�� �ֺ� 9���� ���Ϳ��� �ڽ��� ������ �÷��̾�鿡�� �������Ѵ�
	for (BYTE i = 0; i < attackSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAttackViewer = LinkToClient(pLink);

			// ������ ������ ���� �������Ѵ�
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			// �׳� AttackViewer�� SessionID�� �ʿ��ؼ� ���� ���� �ʾ���.
			Packet* pSC_ATTACK2 = Packet::Alloc();
			MAKE_SC_ATTACK2(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK2);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK2);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK2_RANGE_Y, dfATTACK2_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(pPlayer, dfATTACK2_RANGE_Y, dfATTACK2_RANGE_X, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);

	if (!pVictim)
	{
		ReleaseSRWLockShared(&pPlayer->playerLock);
		return FALSE;
	}

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	// SC_DAMAGE ���� ���� ���ϱ�
	st_SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE���� ���Ͷ� SHARED�� ���� �Ǳ�� ��������Ŷ ������, ���Ͷ� Ǯ��, �ǰ��� Exclusive�� Ǯ��, ��ȯ
	AcquireSectorAroundShared(&damageSectorAround);
	pVictim->chHp -= dfATTACK2_DAMAGE;
	DamageProc(pPlayer, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	ReleaseSRWLockShared(&pPlayer->playerLock);
	return TRUE;
}

BOOL CS_ATTACK3(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);

	// �ǰ������� ������ ��Ŷ�� ���� ���͸� ���ؼ� ���� �Ǵ�
	st_SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// ���ݸ���� ���� SC_ATTACK�� �ֺ� 9���� ���Ϳ��� �ڽ��� ������ �÷��̾�鿡�� �������Ѵ�
	for (BYTE i = 0; i < attackSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAttackViewer = LinkToClient(pLink);

			// ������ ������ ���� �������Ѵ�
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			// �׳� AttackViewer�� SessionID�� �ʿ��ؼ� ���� ���� �ʾ���.
			Packet* pSC_ATTACK3 = Packet::Alloc();
			MAKE_SC_ATTACK3(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK3);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK3);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(pPlayer, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);

	ReleaseSectorAroundShared(&attackSectorAround);

	if (!pVictim)
	{
		ReleaseSRWLockShared(&pPlayer->playerLock);
		return FALSE;
	}

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	// SC_DAMAGE ���� ���� ���ϱ�
	st_SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE���� ���Ͷ� SHARED�� ���� �Ǳ�� ��������Ŷ ������, ���Ͷ� Ǯ��, �ǰ��� Exclusive�� Ǯ��, ��ȯ
	AcquireSectorAroundShared(&damageSectorAround);
	pVictim->chHp -= dfATTACK2_DAMAGE;
	DamageProc(pPlayer, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	ReleaseSRWLockShared(&pPlayer->playerLock);
	return TRUE;
}



BOOL CS_ECHO(st_Client* pClient, DWORD dwTime)
{
	Packet* pSC_ECHO = Packet::Alloc();
	MAKE_SC_ECHO(dwTime, pSC_ECHO);
	g_GameServer.SendPacket(pClient->SessionId, pSC_ECHO);
	return TRUE;
}

