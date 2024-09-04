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
	BOOL bValidVertical;
	BOOL bValidHorizon;
	bValidVertical = (sp.shY >= 0) && (sp.shY < dwNumOfSectorVertical); // Y축 정상
	bValidHorizon = (sp.shX >= 0) && (sp.shX < dwNumOfSectorHorizon); // X축 정상
	return bValidVertical && bValidHorizon; // 둘다 정상이면 TRUE
}

__forceinline BYTE GetSectorMoveDir(SectorPos oldSector, SectorPos newSector)
{
	SHORT shCompY;
	SHORT shCompX;

	shCompX = newSector.shX - oldSector.shX;
	shCompY = newSector.shY - oldSector.shY;
	if (SectorMoveDir[shCompY + 1][shCompX + 1] == 8)
		__debugbreak();
	return SectorMoveDir[shCompY + 1][shCompX + 1];
}

static __forceinline BOOL IsSameSector(SHORT shOldSectorY, SHORT shOldSectorX, SHORT shNewSectorY, SHORT shNewSectorX)
{
	BOOL bRet;
	BOOL bSameY;
	BOOL bSameX;
	bSameY = shOldSectorY == shNewSectorY;
	bSameX = shOldSectorX == shNewSectorX;
	bRet = bSameY && bSameX;
	return bRet;
}

static __forceinline BOOL IsSameSector(SectorPos oldSector, SectorPos newSector)
{
	return oldSector.YX == newSector.YX;
}

static __forceinline BOOL IsValidPos(SHORT shY, SHORT shX)
{
	BOOL bRet;
	BOOL bValidY;
	BOOL bValidX;
	bValidY = (dfRANGE_MOVE_TOP <= shY && shY < dfRANGE_MOVE_BOTTOM);
	bValidX = (dfRANGE_MOVE_LEFT <= shX && shX < dfRANGE_MOVE_RIGHT);
	bRet = bValidY && bValidX;
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
__forceinline void GetDeltaSector(BYTE byBaseDir, st_SECTOR_AROUND* pSectorAround, BYTE byDeltaSectorNum, SHORT shBaseSectorPointY, SHORT shBaseSectorPointX)
{
	SHORT shGetSectorY;
	SHORT shGetSectorX;

	pSectorAround->byCount = 0;
	for (int i = 0; i < byDeltaSectorNum; ++i)
	{
		shGetSectorY = shBaseSectorPointY + vArr[byBaseDir].shY;
		shGetSectorX = shBaseSectorPointX + vArr[byBaseDir].shX;
		byBaseDir = (++byBaseDir) % 8;
		if (IsValidSector(shGetSectorY, shGetSectorX))
		{
			pSectorAround->Around[pSectorAround->byCount].shY = shGetSectorY;
			pSectorAround->Around[pSectorAround->byCount].shX = shGetSectorX;
			++(pSectorAround->byCount);
		}
	}
}

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
		if (IsNetworkStateInValid(pClient->handle))
			goto lb_next;

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
__forceinline void CalcSector(SectorPos* pSP, Pos pos)
{
	pSP->shY = pos.shY / df_SECTOR_HEIGHT;
	pSP->shX = pos.shX / df_SECTOR_WIDTH;
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

void LockAroundSector(SectorPos sector);
void UnLockAroundSector(SectorPos sector);
void AddClientAtSector(st_Client* pClient, SectorPos newSectorPos);
void RemoveClientAtSector(st_Client* pClient, SectorPos oldSectorPos);
AroundInfo* GetAroundValidClient(SectorPos sp, st_Client* pExcept);
AroundInfo* GetDeltaValidClient(BYTE byBaseDir, BYTE byDeltaSectorNum, SectorPos SectorBasePos);
