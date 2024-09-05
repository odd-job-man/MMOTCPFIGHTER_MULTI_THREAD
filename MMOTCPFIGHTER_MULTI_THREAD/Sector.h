#pragma once
#pragma once
#include "Client.h"
#include <cstddef>
#include "Position.h"
#include "Direction.h"
#include "LinkedList.h"

#include "Constant.h"

struct st_Client;

struct st_SECTOR_CLIENT_INFO
{
	LINKED_NODE* pClientLinkHead = nullptr;
	LINKED_NODE* pClientLinkTail = nullptr;
	DWORD dwNumOfClient = 0;
	SRWLOCK srwSectionLock;
};

struct LockInfo
{
	SRWLOCK lockArr[9];
	int iLockNum;
};

extern st_SECTOR_CLIENT_INFO g_Sector[dwNumOfSectorVertical][dwNumOfSectorHorizon];


struct st_MOVEINFO
{
	Pos Prev;
	Pos Cur;
};

struct st_SECTOR_AROUND
{
	SectorPos Around[9];
	BYTE byCount; // 0 ~ 9
};

struct st_DirVector
{
	char byY;
	char byX;
};

union AroundInfo
{
	struct st_AroundClientInfo
	{
		DWORD dwNum;
		st_Client* cArr[7800];
	}CI;
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

// 제거된 섹터를 얻을때는 BaseSector에 OldSector대입
// 새로운 섹터를 얻을때는 BaseSector에 NewSector대입
void GetNewSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, BYTE byDeltaSectorNum, SectorPos nextSector);

__forceinline void GetValidClientFromSector(SectorPos sectorPos, AroundInfo* pAroundInfo, int* pNum, st_Client* pExcept)
{
	LINKED_NODE* pCurLink;
	st_SECTOR_CLIENT_INFO* pSCI;
	st_Client* pClient;

	pSCI = &(g_Sector[sectorPos.shY][sectorPos.shX]);
	pCurLink = pSCI->pClientLinkHead;
	for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
	{
		pClient = LinkToClient(pCurLink);

		// 함수이름앞에 Valid가 붙은 이유
		//if (IsNetworkStateInValid(pClient->handle))
		//	goto lb_next;

		if (pClient == pExcept)
			goto lb_next;

		pAroundInfo->CI.cArr[*pNum] = pClient;
		++(*pNum);

	lb_next:
		pCurLink = pCurLink->pNext;
	}
}

void GetSectorAround(SectorPos CurSector, st_SECTOR_AROUND* pOutSectorAround);


#pragma optimize("",on)
__forceinline SectorPos CalcSector(Pos pos)
{
	SectorPos ret;
	ret.shY = pos.shY / df_SECTOR_HEIGHT;
	ret.shX = pos.shX / df_SECTOR_WIDTH;
	return ret;
}
#pragma optimize("",off)

inline void GetSectorAround(SHORT shPosY, SHORT shPosX, st_SECTOR_AROUND* pOutSectorAround)
{
	int iSectorY;
	int iSectorX;
	int iAroundSectorY;
	int iAroundSectorX;
	SHORT* pCount;

	iSectorY = shPosY / df_SECTOR_HEIGHT;
	iSectorX = shPosX / df_SECTOR_WIDTH;
	pCount = (SHORT*)((char*)pOutSectorAround + offsetof(st_SECTOR_AROUND, byCount));
	*pCount = 0;


	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			iAroundSectorY = iSectorY + dy;
			iAroundSectorX = iSectorX + dx;
			if (IsValidSector(iAroundSectorY, iAroundSectorX))
			{
				pOutSectorAround->Around[*pCount].shY = iAroundSectorY;
				pOutSectorAround->Around[*pCount].shX = iAroundSectorX;
				++(*pCount);
			}
		}
	}
}
void GetNewSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, LockInfo* pOutLockInfo, SectorPos nextSector);
void GetRemoveSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, LockInfo* pOutLockInfo, BYTE byDeltaSectorNum, SectorPos nextSector);
void LockAroundSectorShared(SectorPos sector);
void UnLockAroundSectorShared(SectorPos sector);
void GetMoveLockInfo(LockInfo* pOutLockInfo, SectorPos prevSector, SectorPos afterSector);
void AddClientAtSector(st_Client* pClient, SectorPos newSectorPos);
void RemoveClientAtSector(st_Client* pClient, SectorPos oldSectorPos);
AroundInfo* GetAroundValidClient(SectorPos sp, st_Client* pExcept);
AroundInfo* GetDeltaValidClient(BYTE byBaseDir, BYTE byDeltaSectorNum, SectorPos SectorBasePos);
void SectorUpdateAndNotify(st_Client* pClient, MOVE_DIR sectorMoveDir, SectorPos oldSectorPos, SectorPos newSectorPos, BOOL IsMove);
