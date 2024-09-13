#pragma once
#include "Direction.h"
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

struct START_STOP_INFO
{
	SECTOR_AROUND* pDisappearingSectorS;
	SECTOR_AROUND* pIncomingSectorS;
	SECTOR_AROUND* pMOVE_START_OR_STOP_AROUND;
	SectorPos* pOldSectorPos;
	SectorPos* pNewSectorPos;

	__forceinline BOOL IsSync()
	{
		return !pOldSectorPos && !pNewSectorPos;
	}

	__forceinline BOOL Init(SECTOR_AROUND* pDisappearingSectorS_NULLABLE, SECTOR_AROUND* pIncomingSectorS_NULLABLE, SECTOR_AROUND* pMOVE_START_OR_STOP_AROUND, SectorPos* pOldSectorPos_NULLABLE, SectorPos* pNewSectorPos_NULLABLE)
	{
		this->pDisappearingSectorS = pDisappearingSectorS_NULLABLE;
		this->pIncomingSectorS = pIncomingSectorS_NULLABLE;
		this->pMOVE_START_OR_STOP_AROUND = pMOVE_START_OR_STOP_AROUND;
		this->pOldSectorPos = pOldSectorPos_NULLABLE;
		this->pNewSectorPos = pNewSectorPos_NULLABLE;
		return TRUE;
	}
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

void AcquireSectorAroundShared(SECTOR_AROUND* pSectorAround);
void AcquireSectorAroundExclusive(SECTOR_AROUND* pSectorAround);
BOOL TryAcquireSectorAroundExclusive(SECTOR_AROUND* pSectorAround);
BOOL TryAcquireSectorAroundShared(SECTOR_AROUND* pSectorAround);
BOOL AcquireSectorAroundShared_IF_PLAYER_EXCLUSIVE(Player* pExclusivePlayer, SECTOR_AROUND* pSectorAround);
BOOL AcquireSectorAroundExclusive_IF_PLAYER_EXCLUSIVE(Player* pExclusivePlayer, SECTOR_AROUND* pSectorAround);
void ReleaseSectorAroundExclusive(SECTOR_AROUND* pSectorAround);
void ReleaseSectorAroundShared(SECTOR_AROUND* pSectorAround);
BOOL TryAcquireCreateDeleteSectorLock(SECTOR_AROUND* pSectorAround, SectorPos playerSector);
void ReleaseCreateDeleteSectorLock(SECTOR_AROUND* pSectorAround, SectorPos playerSector);
BOOL AcquireStartStopInfoLock_IF_PLAYER_EXCLUSIVE(Player* pPlayer, START_STOP_INFO* pSSI);
void ReleaseStartStopInfo(START_STOP_INFO* pSSI);

BOOL TryAcquireMoveLock(MOVE_SECTOR_INFO* pMSI);
void ReleaseMoveLock(MOVE_SECTOR_INFO* pMSI);
void ReleaseFailMoveLocks(MOVE_SECTOR_INFO* pMSI, int idx);
void GetMoveSectorS(SECTOR_AROUND* pMoveSectorAround, SectorPos prevSector, SectorPos afterSector);
void GetMoveSectorInfo(MOVE_SECTOR_INFO* pOutMSI, SECTOR_AROUND* pMoveSectorS, SECTOR_AROUND* pDisappearingSectorS, SECTOR_AROUND* pIncomingSectorS);

void GetNewSector(MOVE_DIR dir, SECTOR_AROUND* pOutSectorAround, SectorPos nextSector);
void GetRemoveSector(MOVE_DIR dir, SECTOR_AROUND* pOutSectorAround, SectorPos prevSector);
void AddClientAtSector(Player* pClient, SectorPos newSectorPos);
void RemoveClientAtSector(Player* pClient, SectorPos oldSectorPos);
void BroadcastCreateAndDelete_ON_START_OR_STOP(Player* pPlayer, SECTOR_AROUND* pIncomingSectorS, SECTOR_AROUND* pDisappearingSectorS, BOOL IsMove);
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

