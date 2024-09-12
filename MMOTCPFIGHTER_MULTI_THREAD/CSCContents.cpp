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

__forceinline Player* HitLinkToPlayer(LINKED_NODE* pHitLink)
{
	Player* pRet = (Player*)((char*)pHitLink - offsetof(Player, HitLink));
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

void SyncProc(Player* pSyncPlayer, SectorPos recvSector)
{
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, recvSector);

	AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pSyncPlayer, &sectorAround);
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
			LinkToLinkedListLast(ppHead, ppTail, &pPlayer->HitLink);
			pLink = pLink->pNext;
		}
	}
}

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
			if (IsLLColide(attackerPos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
				return pHitCandidate;

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
			if (IsRRColide(attackerPos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
				return pHitCandidate;

			pHitLink = pHitLink->pNext;
			ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(Player* pPlayer, MOVE_DIR moveDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	SectorPos oldSector = CalcSector(pPlayer->pos);

	// Ŭ���̾�Ʈ �ü�ó��
	ProcessPlayerViewDir(pPlayer, moveDir);
	pPlayer->moveDir = moveDir;

	// �̵��߿� ������ �ٲ㼭 STOP�� �����ʰ� �� �ٽ� START�� �Ӵµ�, �̶� ���� �������� �������� ��ũ�� �߻��ϴ� ���.
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

	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ������ �� �����δٰ� �����ű⿡ ������ �����δٰ� �˸��ʿ� ����.
		SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, FALSE, FALSE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pPlayer, &sectorAround);
	for (int i = 0; i < sectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAncient = LinkToClient(pLink);
			// �� �ڽſ��Դ� START�� �Ⱥ�������, �̰� if�� ��ġ������ �޸� �� ������
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
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
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

	// dfERROR_RANGE���� ������ ������, ���Ͱ� Ʋ���� ���͸� Ŭ��������� ������ ������Ʈ 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// ���� �����̱� ������ ���� ���̴� �ֵ����� �����δٰ� ������ �Ʒ����� �ٽ� Stop ��Ŷ�� �����°� ���̵ȴ�
		SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, TRUE, FALSE);
	}

	// ���� ���Ͷ�� ���� ��ġ�� ���Ϳ� ���� �����̱� �����Ѵٰ� ��Ŷ�� �������Ѵ�.
	SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(pPlayer, &sectorAround);
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
	// ���⼭ �����ڿ��� ���� �� �� ������ ���� ���� ���ɰ� CS_ATTACK�� ���� �� �浹���� ���Ϳ��� �÷��̾�� �ּҸ� ����Ʈ�� �����ؿ���  ������ �������� ��Ǯ�� ������ ��Ǭ��.
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

			// �׳� AttackViewer�� SessionID�� �ʿ��ؼ� ���� ���� �ʾ���.
			Packet* pSC_ATTACK1 = Packet::Alloc();
			MAKE_SC_ATTACK1(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK1);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK1);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround�� AttackSectorAround�� �κ������̾���ϸ� ���� �Ŵµ� ���°Ծƴ϶� �ǰ������� ���� ������ �÷��̾��Ʈ�� ��ȸ�ϴ°����� ����Ѵ�
	SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);

	LINKED_NODE* pHitLinkHead = nullptr;
	LINKED_NODE* pHitLinkTail = nullptr;

	//AcquireSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ATTACK1, SHARED, ACQUIRE);

	CopyHitCandidate(pPlayer, &pHitLinkHead, &pHitLinkTail, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);
	DWORD dwAttackerID = pPlayer->dwID;
	Pos attackerPos = pPlayer->pos;
	MOVE_DIR attackerViewDir = pPlayer->viewDir;
	ReleaseSRWLockShared(&pPlayer->playerLock);

	// ������ �ǰ� �ĺ��� ����Ʈ�� ���鼭 EXCLUSIVE�� ���ɰ� �ǰ������� �ǰݽ� ��ȯ�Ѵ�. �̶� ��ȯ�ȴٸ� EXCLUSIVE���� Ǯ�� ����ä ��ȯ�Ѵ�
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, pHitLinkHead);


	if (!pVictim)
	{
		//ReleaseSRWLockShared(&g_srwPlayerArrLock);
		return FALSE;
	}

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	// SC_DAMAGE ���� ���� ���ϱ�
	SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE_NO_GUARANTEE_ISVALID(pVictim, &damageSectorAround);
	//ReleaseSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ATTACK1, SHARED, RELEASE);

	pVictim->hp -= dfATTACK1_DAMAGE;
	DamageProc(dwAttackerID, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	return TRUE;
}

BOOL CS_ATTACK2(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	// ���⼭ �����ڿ��� ���� �� �� ������ ���� ���� ���ɰ� CS_ATTACK�� ���� �� �浹���� ���Ϳ��� �÷��̾�� �ּҸ� ����Ʈ�� �����ؿ���  ������ �������� ��Ǯ�� ������ ��Ǭ��.
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
	SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK2_RANGE_Y, dfATTACK2_RANGE_X, &collisionSectorAround);

	LINKED_NODE* pHitLinkHead = nullptr;
	LINKED_NODE* pHitLinkTail = nullptr;

	//AcquireSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ATTACK2, SHARED, ACQUIRE);

	CopyHitCandidate(pPlayer, &pHitLinkHead, &pHitLinkTail, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);
	DWORD dwAttackerID = pPlayer->dwID;
	Pos attackerPos = pPlayer->pos;
	MOVE_DIR attackerViewDir = pPlayer->viewDir;
	ReleaseSRWLockShared(&pPlayer->playerLock);

	// ������ �ǰ� �ĺ��� ����Ʈ�� ���鼭 EXCLUSIVE�� ���ɰ� �ǰ������� �ǰݽ� ��ȯ�Ѵ�. �̶� ��ȯ�ȴٸ� EXCLUSIVE���� Ǯ�� ����ä ��ȯ�Ѵ�
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK2_RANGE_Y, dfATTACK2_RANGE_X, pHitLinkHead);

	if (!pVictim)
	{
		//ReleaseSRWLockShared(&g_srwPlayerArrLock);
		return FALSE;
	}

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	// SC_DAMAGE ���� ���� ���ϱ�
	SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE���� ���Ͷ� SHARED�� ���� �Ǳ�� ��������Ŷ ������, ���Ͷ� Ǯ��, �ǰ��� Exclusive�� Ǯ��, ��ȯ
	AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE_NO_GUARANTEE_ISVALID(pVictim, &damageSectorAround);
	//ReleaseSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ATTACK2, SHARED, RELEASE);

	pVictim->hp -= dfATTACK2_DAMAGE;
	DamageProc(dwAttackerID, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	return TRUE;
}

BOOL CS_ATTACK3(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	// ���⼭ �����ڿ��� ���� �� �� ������ ���� ���� ���ɰ� CS_ATTACK�� ���� �� �浹���� ���Ϳ��� �÷��̾�� �ּҸ� ����Ʈ�� �����ؿ���  ������ �������� ��Ǯ�� ������ ��Ǭ��.
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
	SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);

	LINKED_NODE* pHitLinkHead = nullptr;
	LINKED_NODE* pHitLinkTail = nullptr;
	//AcquireSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ATTACK3, SHARED, ACQUIRE);

	CopyHitCandidate(pPlayer, &pHitLinkHead, &pHitLinkTail, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);
	DWORD dwAttackerID = pPlayer->dwID;
	Pos attackerPos = pPlayer->pos;
	MOVE_DIR attackerViewDir = pPlayer->viewDir;
	ReleaseSRWLockShared(&pPlayer->playerLock);
	//WRITE_MEMORY_LOG(ATTACK3, SHARED, RELEASE);

	// ������ �ǰ� �ĺ��� ����Ʈ�� ���鼭 EXCLUSIVE�� ���ɰ� �ǰ������� �ǰݽ� ��ȯ�Ѵ�. �̶� ��ȯ�ȴٸ� EXCLUSIVE���� Ǯ�� ����ä ��ȯ�Ѵ�
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);

	if (!pVictim)
	{
		//ReleaseSRWLockShared(&g_srwPlayerArrLock);
		return FALSE;
	}

	// �� �������� �ǰ��ڴ� EXCLUSIVE�� ���� �ɷ������� Release�� ������ ������ Update�� ���� ���������� �ʴ´�.
	// SC_DAMAGE ���� ���� ���ϱ�
	SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE���� ���Ͷ� SHARED�� ���� �Ǳ�� ��������Ŷ ������, ���Ͷ� Ǯ��, �ǰ��� Exclusive�� Ǯ��, ��ȯ
	AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE_NO_GUARANTEE_ISVALID(pVictim, &damageSectorAround);
	//ReleaseSRWLockShared(&g_srwPlayerArrLock);

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

