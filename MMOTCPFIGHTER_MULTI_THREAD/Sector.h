#pragma once
#include "Client.h"
#include <cstddef>
#include "Position.h"
#include "Direction.h"
#include "LinkedList.h"

#include "Constant.h"
#include "Update.h"
#include <stdio.h>
#include "MemLog.h"

struct Player;

struct st_SECTOR_CLIENT_INFO
{
	LINKED_NODE* pClientLinkHead = nullptr;
	LINKED_NODE* pClientLinkTail = nullptr;
	int iNumOfClient = 0;
	SRWLOCK srwSectionLock;
};

extern st_SECTOR_CLIENT_INFO g_Sector[dwNumOfSectorVertical][dwNumOfSectorHorizon];


struct MOVE_SECTOR_INFO
{
	SectorPos spArr[12];
	int iCnt; // 8 ~ 12;
	BOOL bExclusive[12];
};

struct SECTOR_AROUND
{
	SectorPos Around[9];
	int iCnt; // 0 ~ 9
};

struct st_DirVector
{
	char byY;
	char byX;
};

__forceinline BOOL IsValidSector(int iSectorY, int iSectorX)
{
	BOOL bValidVertical;
	BOOL bValidHorizon;
	bValidVertical = (iSectorY >= 0) && (iSectorY < dwNumOfSectorVertical); // Y축 정상
	bValidHorizon = (iSectorX >= 0) && (iSectorX < dwNumOfSectorHorizon); // X축 정상
	return bValidVertical && bValidHorizon; // 둘다 정상이면 TRUE
}

#pragma optimize("",on)
__forceinline BOOL IsValidSector(SectorPos sp)
{
	BOOL bValidVertical = (sp.shY >= 0) && (sp.shY < dwNumOfSectorVertical); // Y축 정상
	BOOL bValidHorizon = (sp.shX >= 0) && (sp.shX < dwNumOfSectorHorizon); // X축 정상
	return bValidVertical && bValidHorizon; // 둘다 정상이면 TRUE
}

// 이동전 섹터 -> 이동 후 섹터의 방향변화를 구한다
__forceinline MOVE_DIR GetSectorMoveDir(SectorPos oldSector, SectorPos newSector)
{
	SHORT shCompX = newSector.shX - oldSector.shX;
	SHORT shCompY = newSector.shY - oldSector.shY;
	if (SectorMoveDir[shCompY + 1][shCompX + 1] == 8)
		__debugbreak();
	return SectorMoveDir[shCompY + 1][shCompX + 1];
}

__forceinline MOVE_DIR GetOppositeDir(MOVE_DIR dir)
{
	switch (dir)
	{
	case MOVE_DIR_LL:
		return MOVE_DIR_RR;
	case MOVE_DIR_LU:
		return MOVE_DIR_RD;
	case MOVE_DIR_UU:
		return MOVE_DIR_DD;
	case MOVE_DIR_RU:
		return MOVE_DIR_LD;
	case MOVE_DIR_RR:
		return MOVE_DIR_LL;
	case MOVE_DIR_RD:
		return MOVE_DIR_LU;
	case MOVE_DIR_DD:
		return MOVE_DIR_UU;
	case MOVE_DIR_LD:
		return MOVE_DIR_RU;
	case MOVE_DIR_NOMOVE:
		__debugbreak();
		return MOVE_DIR_NOMOVE;
	default:
		__debugbreak();
		return MOVE_DIR_NOMOVE;
	}
}

static __forceinline BOOL IsSameSector(SHORT shOldSectorY, SHORT shOldSectorX, SHORT shNewSectorY, SHORT shNewSectorX)
{
	BOOL bSameY = shOldSectorY == shNewSectorY;
	BOOL bSameX = shOldSectorX == shNewSectorX;
	BOOL bRet = bSameY && bSameX;
	return bRet;
}

static __forceinline BOOL IsSameSector(SectorPos oldSector, SectorPos newSector)
{
	return oldSector.YX == newSector.YX;
}

static __forceinline BOOL IsValidPos(SHORT shY, SHORT shX)
{
	BOOL bValidY = (dfRANGE_MOVE_TOP <= shY && shY < dfRANGE_MOVE_BOTTOM);
	BOOL bValidX = (dfRANGE_MOVE_LEFT <= shX && shX < dfRANGE_MOVE_RIGHT);
	BOOL bRet = bValidY && bValidX;
	return bRet;
}

static __forceinline BOOL IsValidPos(Pos pos)
{
	BOOL bRet;
	BOOL bValidY;
	BOOL bValidX;
	bValidY = (dfRANGE_MOVE_TOP <= pos.shY && pos.shY < dfRANGE_MOVE_BOTTOM);
	bValidX = (dfRANGE_MOVE_LEFT <= pos.shX && pos.shX < dfRANGE_MOVE_RIGHT);
	bRet = bValidY && bValidX;
	return bRet;
}
#pragma optimize("",off)

__forceinline void AcquireSectorAroundShared(SECTOR_AROUND* pSectorAround)
{
	for (int i = 0; i < pSectorAround->iCnt; ++i)
		AcquireSRWLockShared(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock);
}

__forceinline void AcquireSectorAroundExclusive(SECTOR_AROUND* pSectorAround)
{
	for (int i = 0; i < pSectorAround->iCnt; ++i)
		AcquireSRWLockExclusive(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock);
}

__forceinline BOOL TryAcquireSectorAroundExclusive(SECTOR_AROUND* pSectorAround)
{
	for (int i = 0; i < pSectorAround->iCnt; ++i)
	{
		if (TryAcquireSRWLockExclusive(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock))
			continue;

		for (int j = i - 1; j >= 0; --j)
			ReleaseSRWLockExclusive(&g_Sector[pSectorAround->Around[j].shY][pSectorAround->Around[j].shX].srwSectionLock);

		return FALSE;
	}
	return TRUE;
}

__forceinline BOOL TryAcquireSectorAroundShared(SECTOR_AROUND* pSectorAround)
{
	for (int i = 0; i < pSectorAround->iCnt; ++i)
	{
		if (TryAcquireSRWLockShared(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock))
			continue;

		for (int j = i - 1; j >= 0; --j)
			ReleaseSRWLockShared(&g_Sector[pSectorAround->Around[j].shY][pSectorAround->Around[j].shX].srwSectionLock);

		return FALSE;
	}
	return TRUE;
}

__forceinline void AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE_NO_GUARANTEE_ISVALID(Player* pExclusivePlayer, SECTOR_AROUND* pSectorAround)
{
	do
	{
		if (TryAcquireSectorAroundShared(pSectorAround))
			break;

		ReleaseSRWLockExclusive(&pExclusivePlayer->playerLock);
		AcquireSRWLockExclusive(&pExclusivePlayer->playerLock);
	} while (true);
}

__forceinline void AcquireSectorAroundExclusive_IF_PLAYER_EXCLUSIVE_NO_GUARANTEE_ISVALID(Player* pExclusivePlayer, SECTOR_AROUND* pSectorAround)
{
	// 락을 얻는동안 플레이어가 OnRelease의 호출로 인해 없어지는것을 막는다.
	do
	{
		if (TryAcquireSectorAroundExclusive(pSectorAround))
			break;

		ReleaseSRWLockExclusive(&pExclusivePlayer->playerLock);
		AcquireSRWLockExclusive(&pExclusivePlayer->playerLock);
	} while (true);
}

__forceinline void AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(Player* pExclusivePlayer, SECTOR_AROUND* pSectorAround)
{
	// 락을 얻는동안 플레이어가 OnRelease의 호출로 인해 없어지는것을 막는다.
	//AcquireSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ACQ_SECTOR_AROUND_SHARED_IF_PLAYER_EXCLUSIVE, SHARED, ACQUIRE);
	do
	{
		if (TryAcquireSectorAroundShared(pSectorAround))
		{
			//ReleaseSRWLockShared(&g_srwPlayerArrLock);
			break;
		}

		ReleaseSRWLockExclusive(&pExclusivePlayer->playerLock);
		AcquireSRWLockExclusive(&pExclusivePlayer->playerLock);
	} while (true);
}

__forceinline void AcquireSectorAroundExclusive_IF_PLAYER_EXCLUSIVE(Player* pExclusivePlayer, SECTOR_AROUND* pSectorAround)
{
	// 락을 얻는동안 플레이어가 OnRelease의 호출로 인해 없어지는것을 막는다.
	//AcquireSRWLockShared(&g_srwPlayerArrLock);
	//WRITE_MEMORY_LOG(ACQ_SECTOR_AROUND_EXCLUSIVE_IF_PLAYER_EXCLUSIVE, SHARED, ACQUIRE);
	do
	{
		if (TryAcquireSectorAroundExclusive(pSectorAround))
		{
			//ReleaseSRWLockShared(&g_srwPlayerArrLock);
			break;
		}

		ReleaseSRWLockExclusive(&pExclusivePlayer->playerLock);
		AcquireSRWLockExclusive(&pExclusivePlayer->playerLock);
	} while (true);
}

__forceinline void ReleaseSectorAroundExclusive(SECTOR_AROUND* pSectorAround)
{
	for (int i = pSectorAround->iCnt - 1; i >= 0; --i)
		ReleaseSRWLockExclusive(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock);
}

__forceinline void ReleaseSectorAroundShared(SECTOR_AROUND* pSectorAround)
{
	for (int i = pSectorAround->iCnt - 1; i >= 0; --i)
		ReleaseSRWLockShared(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock);
}

__forceinline BOOL TryAcquireCreateDeleteSectorLock(SECTOR_AROUND* pSectorAround, SectorPos playerSector)
{
	for (int i = 0; i < pSectorAround->iCnt; ++i)
	{
		if (pSectorAround->Around[i].YX == playerSector.YX)
		{
			if (TryAcquireSRWLockExclusive(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock))
				continue;
		}
		else
		{
			if (TryAcquireSRWLockShared(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock))
				continue;
		}

		for (int j = i - 1; j >= 0; --j)
		{
			if (pSectorAround->Around[j].YX == playerSector.YX)
				ReleaseSRWLockExclusive(&g_Sector[pSectorAround->Around[j].shY][pSectorAround->Around[j].shX].srwSectionLock);
			else
				ReleaseSRWLockShared(&g_Sector[pSectorAround->Around[j].shY][pSectorAround->Around[j].shX].srwSectionLock);
		}
		return FALSE;
	}
	return TRUE;
}

__forceinline void ReleaseCreateDeleteSectorLock(SECTOR_AROUND* pSectorAround, SectorPos playerSector)
{
	for (int i = pSectorAround->iCnt - 1; i >= 0; --i)
	{
		if (pSectorAround->Around[i].YX == playerSector.YX)
			ReleaseSRWLockExclusive(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock);
		else
			ReleaseSRWLockShared(&g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX].srwSectionLock);
	}
}

void GetSectorAround(SECTOR_AROUND* pOutSectorAround, SectorPos CurSector);


#pragma optimize("",on)
__forceinline SectorPos CalcSector(Pos pos)
{
	SectorPos ret;
	ret.shY = pos.shY / df_SECTOR_HEIGHT;
	ret.shX = pos.shX / df_SECTOR_WIDTH;
	return ret;
}
#pragma optimize("",off)

void GetNewSector(MOVE_DIR dir, SECTOR_AROUND* pOutSectorAround, SectorPos nextSector);
void GetRemoveSector(MOVE_DIR dir, SECTOR_AROUND* pOutSectorAround, SectorPos prevSector);
void GetMoveLockInfo(SECTOR_AROUND* pMoveSectorAround, SectorPos prevSector, SectorPos afterSector);
void AddClientAtSector(Player* pClient, SectorPos newSectorPos);
void RemoveClientAtSector(Player* pClient, SectorPos oldSectorPos);
void SectorUpdateAndNotify(Player* pPlayer, MOVE_DIR sectorMoveDir, SectorPos oldSectorPos, SectorPos newSectorPos, BOOL IsMove, BOOL IsUpdate);
