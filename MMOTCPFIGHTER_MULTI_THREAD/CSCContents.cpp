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
#include "Update.h"

#include <stdio.h>
extern GameServer g_GameServer;

UINT64 g_SyncCnt = 0;

__forceinline Player* HitLinkToPlayer(LINKED_NODE* pHitLink)
{
	Player* pRet = (Player*)((char*)pHitLink - offsetof(Player, hitInfo.hitLink));
	return pRet;
}

__forceinline Player* HitInfoToPlayer(HitInfo* pHitInfo)
{
	Player* pRet = (Player*)((char*)pHitInfo- offsetof(Player, hitInfo));
	return pRet;
}


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

// �����̱� ������ �÷��̾� �ü�ó��
__forceinline void ProcessPlayerViewDir(Player* pMoveStartPlayer, MOVE_DIR moveDir)
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

BOOL SyncProc(Player* pSyncPlayer,SectorPos recvSector)
{
	InterlockedIncrement(&g_SyncCnt);
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, recvSector);

	if (AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pSyncPlayer, &sectorAround) == FALSE)
		return FALSE;

	for (int i = 0; i < sectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int i = 0; i < pSCI->iNumOfClient; ++i)
		{
			// ���� �������� ���� �ű��մ� ���ֹ� and ��ũ ����ڿ��Ե� �������ϸ� �� �������� SessionId�� �ʿ��ؼ� ���� ���� �ʾ���
			Player* pAncient = LinkToClient(pLink);
			Packet* pSyncPacket = Packet::Alloc();
			MAKE_SC_SYNC(pSyncPlayer->dwID, pSyncPlayer->pos, pSyncPacket);
			g_GameServer.SendPacket(pAncient->SessionId, pSyncPacket);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	return TRUE;
}

void FindSectorInAttackRange(Pos pos, MOVE_DIR viewDir, SHORT shAttackRangeY, SHORT shAttackRangeX, SECTOR_AROUND* pSectorAround)
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
	pSectorAround->iCnt = iCount;
}

void CopyHitCandidate(Player* pAttacker, LINKED_NODE** ppHead, LINKED_NODE** ppTail, SECTOR_AROUND* pCollisionSectorAround)
{
	for (int i = 0; i < pCollisionSectorAround->iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pCollisionSectorAround->Around[i].shY][pCollisionSectorAround->Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pPlayer = LinkToClient(pLink);
			if (pAttacker == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}
			pPlayer->hitInfo.BackUpSessionId = pPlayer->SessionId;
			LinkToLinkedListLast(ppHead, ppTail, &pPlayer->hitInfo.hitLink);
			pLink = pLink->pNext;
		}
	}
}

// �ش��Լ� ȣ������ ������ �ɾ���� ���� S���� Ǯ���� ������ �ǰ��ĺ��ڵ��� Release�Ǿ����� �մ�. �̸� ���ؼ� CopyCandidate������ SessionId�� �̸� ����صξ��⿡ �װ��� ������ ��ȿ�� �˻縦�ؼ� ��ȿ���������� �ѱ��
// �ǰݽ� ���� Ǯ�� �ʴ� ������ �� ���̿� OnRelease�� ���ؼ� �������ų� Ȥ�� �׷����� �ٸ� Ŭ��� �ٲ�°��� ���� �����̴�.
Player* HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(Pos attackerPos, MOVE_DIR attackerViewDir, SHORT shAttackRangeY, SHORT shAttackRangeX, LINKED_NODE* pHitLinkHead)
{
	LINKED_NODE* pHitLink = pHitLinkHead;
	if (attackerViewDir == MOVE_DIR_LL)
	{
		while (pHitLink)
		{
			Player* pHitCandidate = HitLinkToPlayer(pHitLink);
			AcquireSRWLockExclusive(&pHitCandidate->playerLock);

			// ��ȿ���� ������ �ѱ��
			if (!g_GameServer.IsPlayerValid(pHitCandidate->hitInfo.BackUpSessionId))
			{
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
				pHitLink = pHitLink->pNext;
				continue;
			}

			// �浹�ϸ� ��Ǯ���ʰ� �״�� ��ȯ�Ѵ�
			if (IsLLColide(attackerPos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
				return pHitCandidate;

			// ��ȿ������ �ǰ��ڰ� �ƴϸ� �ѱ��
			pHitLink = pHitLink->pNext;
			ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
		}
	}
	else
	{
		while (pHitLink)
		{
			Player* pHitCandidate = HitLinkToPlayer(pHitLink);
			AcquireSRWLockExclusive(&pHitCandidate->playerLock);

			// ��ȿ���� ������ �ѱ��
			if (!g_GameServer.IsPlayerValid(pHitCandidate->hitInfo.BackUpSessionId))
			{
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
				pHitLink = pHitLink->pNext;
				continue;
			}

			// �浹�ϸ� ��Ǯ���ʰ� �״�� ��ȯ�Ѵ�
			if (IsRRColide(attackerPos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
				return pHitCandidate;

			// ��ȿ������ �ǰ��ڰ� �ƴϸ� �ѱ��
			pHitLink = pHitLink->pNext;
			ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(Player* pPlayer, MOVE_DIR moveDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	pPlayer->bMoveOrStopInProgress = TRUE;
	SectorPos oldSector = CalcSector(pPlayer->pos);

	// Ŭ���̾�Ʈ �ü�ó��
	ProcessPlayerViewDir(pPlayer, moveDir);
	pPlayer->moveDir = moveDir;

	do
	{
		if (IsSync(pPlayer->pos, playerPos) == FALSE)
			break;

		if (SyncProc(pPlayer, oldSector) == FALSE)
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
		playerPos = pPlayer->pos;
	} while (0);
	pPlayer->pos = playerPos;

	SectorPos newSector = CalcSector(playerPos);
	do
	{
		if (IsSameSector(oldSector, newSector))
			break;

		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ������ �� �����δٰ� �����ű⿡ ������ �����δٰ� �˸��ʿ� ����.
		if (!SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, FALSE))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
	} while (0);


	//�����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	if (AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pPlayer, &sectorAround) == FALSE)
	{
		ReleaseSRWLockExclusive(&pPlayer->playerLock);
		return FALSE;
	}

	for (int i = 0; i < sectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAncient = LinkToClient(pLink);
			if (pAncient == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			Packet* pSC_MOVE_START = Packet::Alloc();
			MAKE_SC_MOVE_START(pPlayer->dwID, pPlayer->moveDir, playerPos, pSC_MOVE_START);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_START);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	pPlayer->bMoveOrStopInProgress = FALSE;
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	pPlayer->bMoveOrStopInProgress = TRUE;
	SectorPos oldSector = CalcSector(pPlayer->pos);
	pPlayer->moveDir = MOVE_DIR_NOMOVE;
	do
	{
		if (IsSync(pPlayer->pos, playerPos) == FALSE)
			break;

		if (SyncProc(pPlayer, oldSector) == FALSE)
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
		playerPos = pPlayer->pos;
	} while (0);
	pPlayer->pos = playerPos;

	SectorPos newSector = CalcSector(playerPos);
	do
	{
		if (IsSameSector(oldSector, newSector))
			break;

		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		if (SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, TRUE) == FALSE)
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
	} while (0);

	// ����ٰ� ��Ŷ�� ������
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	if (AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pPlayer, &sectorAround) == FALSE)
	{
		ReleaseSRWLockExclusive(&pPlayer->playerLock);
		return FALSE;
	}

	for (int i = 0; i < sectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAncient = LinkToClient(pLink);
			if (pAncient == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			Packet* pSC_MOVE_STOP = Packet::Alloc();
			MAKE_SC_MOVE_STOP(pPlayer->dwID, pPlayer->viewDir, playerPos, pSC_MOVE_STOP);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_STOP);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	pPlayer->bMoveOrStopInProgress = FALSE;
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
    return TRUE;
}

__forceinline void DamageProc(DWORD dwAtttackerID, Player* pVictim, SECTOR_AROUND* pSectorAround)
{
	for (int i = 0; i < pSectorAround->iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int i = 0; i < pSCI->iNumOfClient; ++i)
		{
			Player* pDamageViewer = LinkToClient(pLink);
			Packet* pSC_DAMAGE = Packet::Alloc();
			MAKE_SC_DAMAGE(dwAtttackerID, pVictim->dwID, pVictim->hp, pSC_DAMAGE);

			g_GameServer.SendPacket(pDamageViewer->SessionId, pSC_DAMAGE);
			pLink = pLink->pNext;
		}
	}
}

BOOL CS_ATTACK1(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);
	SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// ���ݸ���� ���� SC_ATTACK�� �ֺ� 9���� ���Ϳ��� �ڽ��� ������ �÷��̾�鿡�� �������Ѵ�
	for (int i = 0; i < attackSectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAttackViewer = LinkToClient(pLink);
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}
			Packet* pSC_ATTACK1 = Packet::Alloc();
			MAKE_SC_ATTACK1(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK1);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK1);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);

	LINKED_NODE* pHitLinkHead = nullptr;
	LINKED_NODE* pHitLinkTail = nullptr;
	CopyHitCandidate(pPlayer, &pHitLinkHead, &pHitLinkTail, &collisionSectorAround);

	ReleaseSectorAroundShared(&attackSectorAround);
	DWORD dwAttackerID = pPlayer->dwID;
	Pos attackerPos = pPlayer->pos;
	MOVE_DIR attackerViewDir = pPlayer->viewDir;
	ReleaseSRWLockShared(&pPlayer->playerLock);

	// ������ �ǰ� �ĺ��� ����Ʈ�� ���鼭 EXCLUSIVE�� ���ɰ� �ǰ������� �ǰݽ� ��ȯ�Ѵ�. �̶� ��ȯ�ȴٸ� EXCLUSIVE���� Ǯ�� ����ä ��ȯ�Ѵ�
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);
	if (!pVictim)
		return FALSE;

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));
	if (AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pVictim, &damageSectorAround) == FALSE)
	{
		ReleaseSRWLockExclusive(&pVictim->playerLock);
		return FALSE;
	}

	pVictim->hp -= dfATTACK1_DAMAGE;
	DamageProc(dwAttackerID, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	return TRUE;
}

BOOL CS_ATTACK2(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);
	SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// ���ݸ���� ���� SC_ATTACK�� �ֺ� 9���� ���Ϳ��� �ڽ��� ������ �÷��̾�鿡�� �������Ѵ�
	for (int i = 0; i < attackSectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAttackViewer = LinkToClient(pLink);
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}
			Packet* pSC_ATTACK2 = Packet::Alloc();
			MAKE_SC_ATTACK2(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK2);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK2);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);

	LINKED_NODE* pHitLinkHead = nullptr;
	LINKED_NODE* pHitLinkTail = nullptr;
	CopyHitCandidate(pPlayer, &pHitLinkHead, &pHitLinkTail, &collisionSectorAround);

	ReleaseSectorAroundShared(&attackSectorAround);
	DWORD dwAttackerID = pPlayer->dwID;
	Pos attackerPos = pPlayer->pos;
	MOVE_DIR attackerViewDir = pPlayer->viewDir;
	ReleaseSRWLockShared(&pPlayer->playerLock);

	// ������ �ǰ� �ĺ��� ����Ʈ�� ���鼭 EXCLUSIVE�� ���ɰ� �ǰ������� �ǰݽ� ��ȯ�Ѵ�. �̶� ��ȯ�ȴٸ� EXCLUSIVE���� Ǯ�� ����ä ��ȯ�Ѵ�
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);
	if (!pVictim)
		return FALSE;

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));
	if (AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pVictim, &damageSectorAround) == FALSE)
	{
		ReleaseSRWLockExclusive(&pVictim->playerLock);
		return FALSE;
	}

	pVictim->hp -= dfATTACK2_DAMAGE;
	DamageProc(dwAttackerID, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	return TRUE;
}

BOOL CS_ATTACK3(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);
	SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// ���ݸ���� ���� SC_ATTACK�� �ֺ� 9���� ���Ϳ��� �ڽ��� ������ �÷��̾�鿡�� �������Ѵ�
	for (int i = 0; i < attackSectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAttackViewer = LinkToClient(pLink);
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}
			Packet* pSC_ATTACK3 = Packet::Alloc();
			MAKE_SC_ATTACK3(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK3);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK3);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);

	LINKED_NODE* pHitLinkHead = nullptr;
	LINKED_NODE* pHitLinkTail = nullptr;
	CopyHitCandidate(pPlayer, &pHitLinkHead, &pHitLinkTail, &collisionSectorAround);

	ReleaseSectorAroundShared(&attackSectorAround);
	DWORD dwAttackerID = pPlayer->dwID;
	Pos attackerPos = pPlayer->pos;
	MOVE_DIR attackerViewDir = pPlayer->viewDir;
	ReleaseSRWLockShared(&pPlayer->playerLock);

	// ������ �ǰ� �ĺ��� ����Ʈ�� ���鼭 EXCLUSIVE�� ���ɰ� �ǰ������� �ǰݽ� ��ȯ�Ѵ�. �̶� ��ȯ�ȴٸ� EXCLUSIVE���� Ǯ�� ����ä ��ȯ�Ѵ�
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);
	if (!pVictim)
		return FALSE;

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));
	if (AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pVictim, &damageSectorAround) == FALSE)
	{
		ReleaseSRWLockExclusive(&pVictim->playerLock);
		return FALSE;
	}

	pVictim->hp -= dfATTACK3_DAMAGE;
	DamageProc(dwAttackerID, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	return TRUE;

}


BOOL CS_ECHO(Player* pClient, DWORD dwTime)
{
	Packet* pSC_ECHO = Packet::Alloc();
	MAKE_SC_ECHO(dwTime, pSC_ECHO);
	g_GameServer.SendPacket(pClient->SessionId, pSC_ECHO);
	return TRUE;
}

