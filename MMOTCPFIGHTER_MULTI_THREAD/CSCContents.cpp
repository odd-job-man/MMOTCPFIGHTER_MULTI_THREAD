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

// 움직이기 시작한 플레이어 시선처리
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

void SyncProc(Player* pSyncPlayer, SECTOR_AROUND* pSectorAround_SIMULATED_BY_SERVER)
{
	InterlockedIncrement(&g_SyncCnt);
	for (int i = 0; i < pSectorAround_SIMULATED_BY_SERVER->iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround_SIMULATED_BY_SERVER->Around[i].shY][pSectorAround_SIMULATED_BY_SERVER->Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int i = 0; i < pSCI->iNumOfClient; ++i)
		{
			// 서버 기준으로 원래 거기잇던 원주민 and 싱크 당사자에게도 보내야하며 이 과정에서 SessionId만 필요해서 락을 걸지 않앗음
			Player* pAncient = LinkToClient(pLink);
			Packet* pSyncPacket = Packet::Alloc();
			MAKE_SC_SYNC(pSyncPlayer->dwID, pSyncPlayer->pos, pSyncPacket);
			g_GameServer.SendPacket(pAncient->SessionId, pSyncPacket);
			pLink = pLink->pNext;
		}
	}
}

void FindSectorInAttackRange(Pos pos, MOVE_DIR viewDir, SHORT shAttackRangeY, SHORT shAttackRangeX, SECTOR_AROUND* pSectorAround)
{
	SectorPos originalSector = CalcSector(pos);
	SectorPos temp;

	pSectorAround->Around[0].YX = originalSector.YX;
	int iCount = 1;

	if (viewDir == MOVE_DIR_LL)
	{
		// 왼쪽 방향 공격
		// temp.shX 왼쪽 섹터들의 X값 공통 X값.

		temp.shX = (pos.shX - shAttackRangeX) / df_SECTOR_WIDTH;
		// 바로왼쪽 값이 유효한 섹터임과 동시에 공격범위임
		if (IsValidSector(originalSector.shY, temp.shX) && originalSector.shX != temp.shX)
		{
			pSectorAround->Around[iCount].shX = temp.shX;
			pSectorAround->Around[iCount].shY = originalSector.shY;
			++iCount;

			// 이시점에서 왼쪽은 유효하며 위, 아래 확인해야함
			temp.shY = (pos.shY + shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				// 위
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// 왼쪽 위
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}

			// 아래 확인
			temp.shY = (pos.shY - shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				//아래
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// 왼쪽 아래
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}
		}
		else
		{
			// 왼쪽방향이 다른섹터로 넘어가지 않으면 위 아래만 탐색하면됨
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
		// 오른쪽 방향 공격
		// 오른쪽 방향 섹터검색
		temp.shX = (pos.shX + shAttackRangeX) / df_SECTOR_WIDTH;
		if (IsValidSector(originalSector.shY, temp.shX) && originalSector.shX != temp.shX)
		{
			// 오른쪽 확정 위 아래 검사
			pSectorAround->Around[iCount].shX = temp.shX;
			pSectorAround->Around[iCount].shY = originalSector.shY;
			++iCount;

			// 이시점에서 오른쪽은 유효하며 위, 아래 확인해야함
			temp.shY = (pos.shY + shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				// 위
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// 왼쪽 위
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}

			// 아래 확인
			temp.shY = (pos.shY - shAttackRangeY / 2) / df_SECTOR_HEIGHT;
			if (IsValidSector(temp.shY, originalSector.shX) && originalSector.shY != temp.shY)
			{
				//아래
				pSectorAround->Around[iCount].shX = originalSector.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;

				// 왼쪽 아래
				pSectorAround->Around[iCount].shX = temp.shX;
				pSectorAround->Around[iCount].shY = temp.shY;
				++iCount;
			}

		}
		else
		{
			// 오른쪽방향이 다른섹터로 넘어가지 않으면 위 아래만 탐색하면됨
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

// 해당함수 호출전에 공격자 걸어놓은 섹터 S락을 풀엇기 때문에 피격후보자들이 Release되엇을수 잇다. 이를 위해서 CopyCandidate에서는 SessionId를 미리 백업해두엇기에 그것을 가지고 유효성 검사를해서 유효하지않으면 넘긴다
// 피격시 락을 풀지 않는 이유는 그 사이에 OnRelease로 인해서 없어지거나 혹은 그로인해 다른 클라로 바뀌는것을 막기 위함이다.
Player* HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(Pos attackerPos, MOVE_DIR attackerViewDir, SHORT shAttackRangeY, SHORT shAttackRangeX, LINKED_NODE* pHitLinkHead)
{
	LINKED_NODE* pHitLink = pHitLinkHead;
	if (attackerViewDir == MOVE_DIR_LL)
	{
		while (pHitLink)
		{
			Player* pHitCandidate = HitLinkToPlayer(pHitLink);
			AcquireSRWLockExclusive(&pHitCandidate->playerLock);

			// 유효하지 않으면 넘긴다
			if (!g_GameServer.IsPlayerValid(pHitCandidate->hitInfo.BackUpSessionId))
			{
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
				pHitLink = pHitLink->pNext;
				continue;
			}

			// 충돌하면 락풀지않고 그대로 반환한다
			if (IsLLColide(attackerPos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
				return pHitCandidate;

			// 유효하지만 피격자가 아니면 넘긴다
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

			// 유효하지 않으면 넘긴다
			if (!g_GameServer.IsPlayerValid(pHitCandidate->hitInfo.BackUpSessionId))
			{
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
				pHitLink = pHitLink->pNext;
				continue;
			}

			// 충돌하면 락풀지않고 그대로 반환한다
			if (IsRRColide(attackerPos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
				return pHitCandidate;

			// 유효하지만 피격자가 아니면 넘긴다
			pHitLink = pHitLink->pNext;
			ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(Player* pPlayer, MOVE_DIR moveDir, Pos playerPos)
{
	pPlayer->moveDir = moveDir;
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	SectorPos oldSector = CalcSector(pPlayer->pos);
	SectorPos newSector = CalcSector(playerPos);

	SECTOR_AROUND MOVE_START_AROUND;
	SECTOR_AROUND disappearingSectorS;
	SECTOR_AROUND IncomingSectorS;
	START_STOP_INFO ssi;


	// 플레이어 시선처리
	ProcessPlayerViewDir(pPlayer, moveDir);
	BOOL bSync = IsSync(pPlayer->pos, playerPos);

	if (IsSameSector(oldSector, newSector) || bSync)
	{
		GetSectorAround(&MOVE_START_AROUND, oldSector);
		ssi.Init(nullptr, nullptr, &MOVE_START_AROUND, nullptr, nullptr);
		if (!AcquireStartStopInfoLock_IF_PLAYER_EXCLUSIVE(pPlayer, &ssi))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
		if (bSync)
			SyncProc(pPlayer, &MOVE_START_AROUND);
		newSector = oldSector;
	}
	else
	{
		pPlayer->pos = playerPos;
		GetSectorAround(&MOVE_START_AROUND, newSector);
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		GetRemoveSector(sectorMoveDir, &disappearingSectorS, oldSector);
		GetNewSector(sectorMoveDir, &IncomingSectorS, newSector);
		ssi.Init(&disappearingSectorS, &IncomingSectorS, &MOVE_START_AROUND, &oldSector, &newSector);
		if (!AcquireStartStopInfoLock_IF_PLAYER_EXCLUSIVE(pPlayer, &ssi))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
		RemoveClientAtSector(pPlayer, oldSector);
		AddClientAtSector(pPlayer,newSector);
		BroadcastCreateAndDelete_ON_START_OR_STOP(pPlayer, &IncomingSectorS, &disappearingSectorS, FALSE);
	}

	//움직이기 시작한다고 패킷을 보내야한다.
	for (int i = 0; i < MOVE_START_AROUND.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[MOVE_START_AROUND.Around[i].shY][MOVE_START_AROUND.Around[i].shX];
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
	ReleaseStartStopInfo(&ssi);
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(Player* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	pPlayer->moveDir = MOVE_DIR_NOMOVE;
	pPlayer->viewDir = viewDir;

	SectorPos oldSector = CalcSector(pPlayer->pos);
	SectorPos newSector = CalcSector(playerPos);

	SECTOR_AROUND MOVE_STOP_AROUND;
	SECTOR_AROUND disappearingSectorS;
	SECTOR_AROUND IncomingSectorS;
	START_STOP_INFO ssi;

	BOOL bSync = IsSync(pPlayer->pos, playerPos);
	if (IsSameSector(oldSector,newSector) || IsSync(pPlayer->pos, playerPos))
	{
		GetSectorAround(&MOVE_STOP_AROUND, oldSector);
		ssi.Init(nullptr, nullptr, &MOVE_STOP_AROUND, nullptr, nullptr);
		if (!AcquireStartStopInfoLock_IF_PLAYER_EXCLUSIVE(pPlayer, &ssi))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
		if (bSync)
			SyncProc(pPlayer, &MOVE_STOP_AROUND);
		newSector = oldSector;
	}
	else
	{
		pPlayer->pos = playerPos;
		GetSectorAround(&MOVE_STOP_AROUND, newSector);
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		GetRemoveSector(sectorMoveDir, &disappearingSectorS, oldSector);
		GetNewSector(sectorMoveDir, &IncomingSectorS, newSector);
		ssi.Init(&disappearingSectorS, &IncomingSectorS, &MOVE_STOP_AROUND, &oldSector, &newSector);
		if (!AcquireStartStopInfoLock_IF_PLAYER_EXCLUSIVE(pPlayer, &ssi))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			return FALSE;
		}
		RemoveClientAtSector(pPlayer, oldSector);
		AddClientAtSector(pPlayer,newSector);
		BroadcastCreateAndDelete_ON_START_OR_STOP(pPlayer, &IncomingSectorS, &disappearingSectorS, TRUE);
	}

	for (int i = 0; i < MOVE_STOP_AROUND.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[MOVE_STOP_AROUND.Around[i].shY][MOVE_STOP_AROUND.Around[i].shX];
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
	ReleaseStartStopInfo(&ssi);
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

	// 공격모션을 위해 SC_ATTACK을 주변 9개의 섹터에서 자신을 제외한 플레이어들에게 보내야한다
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

	// collisionSectorAround는 AttackSectorAround의 부분집합이어야하며 락을 거는데 쓰는게아니라 피격판정을 위해 섹터의 플레이어리스트를 순회하는곳에만 사용한다
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

	// 이전의 피격 후보자 리스트를 돌면서 EXCLUSIVE로 락걸고 피격판정후 피격시 반환한다. 이때 반환된다면 EXCLUSIVE락을 풀지 않은채 반환한다
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);
	if (!pVictim)
		return FALSE;

	// 이 시점에서 피격자는 EXCLUSIVE로 락이 걸려잇으니 Release될 위험이 없으며 Update에 의해 움직여지지 않는다.
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

	// 공격모션을 위해 SC_ATTACK을 주변 9개의 섹터에서 자신을 제외한 플레이어들에게 보내야한다
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

	// collisionSectorAround는 AttackSectorAround의 부분집합이어야하며 락을 거는데 쓰는게아니라 피격판정을 위해 섹터의 플레이어리스트를 순회하는곳에만 사용한다
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

	// 이전의 피격 후보자 리스트를 돌면서 EXCLUSIVE로 락걸고 피격판정후 피격시 반환한다. 이때 반환된다면 EXCLUSIVE락을 풀지 않은채 반환한다
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);
	if (!pVictim)
		return FALSE;

	// 이 시점에서 피격자는 EXCLUSIVE로 락이 걸려잇으니 Release될 위험이 없으며 Update에 의해 움직여지지 않는다.
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

	// 공격모션을 위해 SC_ATTACK을 주변 9개의 섹터에서 자신을 제외한 플레이어들에게 보내야한다
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

	// collisionSectorAround는 AttackSectorAround의 부분집합이어야하며 락을 거는데 쓰는게아니라 피격판정을 위해 섹터의 플레이어리스트를 순회하는곳에만 사용한다
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

	// 이전의 피격 후보자 리스트를 돌면서 EXCLUSIVE로 락걸고 피격판정후 피격시 반환한다. 이때 반환된다면 EXCLUSIVE락을 풀지 않은채 반환한다
	Player* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(attackerPos, attackerViewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, pHitLinkHead);
	if (!pVictim)
		return FALSE;

	// 이 시점에서 피격자는 EXCLUSIVE로 락이 걸려잇으니 Release될 위험이 없으며 Update에 의해 움직여지지 않는다.
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

