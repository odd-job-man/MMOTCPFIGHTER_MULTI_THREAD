#include <WinSock2.h>
#include <emmintrin.h>
#include "Sector.h"
#include "Packet.h"
#include "SCCContents.h"
#include "Stack.h"
#include "GameServer.h"

extern GameServer g_GameServer;

AroundInfo g_AroundInfo;
st_SECTOR_CLIENT_INFO g_Sector[dwNumOfSectorVertical][dwNumOfSectorHorizon];

void GetMoveLockInfo(LockInfo* pLockInfo, SectorPos prevSector, SectorPos afterSector)
{
	pLockInfo->iLockNum = 2;
	// Y가 더 큰쪽이 뒤에 들어감
	if (prevSector.shY < afterSector.shY)
	{
		pLockInfo->lockArr[0] = g_Sector[prevSector.shY][prevSector.shX].srwSectionLock;
		pLockInfo->lockArr[1] = g_Sector[afterSector.shY][afterSector.shX].srwSectionLock;
		return;
	}

	if (prevSector.shY > afterSector.shY)
	{
		pLockInfo->lockArr[0] = g_Sector[afterSector.shY][afterSector.shX].srwSectionLock;
		pLockInfo->lockArr[1] = g_Sector[prevSector.shY][prevSector.shX].srwSectionLock;
		return;
	}

	// Y는 같을때 X끼리 비교해서 결정, X가 큰쪽이 더 뒤에 들어감
	if (prevSector.shX < afterSector.shX)
	{
		pLockInfo->lockArr[0] = g_Sector[prevSector.shY][prevSector.shX].srwSectionLock;
		pLockInfo->lockArr[1] = g_Sector[afterSector.shY][afterSector.shX].srwSectionLock;
	}
	else
	{
		pLockInfo->lockArr[0] = g_Sector[afterSector.shY][afterSector.shX].srwSectionLock;
		pLockInfo->lockArr[1] = g_Sector[prevSector.shY][prevSector.shX].srwSectionLock;
	}
}

void InitSectionLock()
{
	for (DWORD i = 0; i < dwNumOfSectorVertical; ++i)
	{
		for (DWORD j = 0; j < dwNumOfSectorHorizon; ++i)
		{
			InitializeSRWLock(&g_Sector[i][j].srwSectionLock);
		}
	}
}

void AddClientAtSector(st_Client* pClient, SectorPos newSectorPos)
{
	LinkToLinkedListLast(&(g_Sector[newSectorPos.shY][newSectorPos.shX].pClientLinkHead), &(g_Sector[newSectorPos.shY][newSectorPos.shX].pClientLinkTail), &(pClient->SectorLink));
	++(g_Sector[newSectorPos.shY][newSectorPos.shX].dwNumOfClient);
}

void RemoveClientAtSector(st_Client* pClient, SectorPos oldSectorPos)
{
	UnLinkFromLinkedList(&(g_Sector[oldSectorPos.shY][oldSectorPos.shX].pClientLinkHead), &(g_Sector[oldSectorPos.shY][oldSectorPos.shX].pClientLinkTail), &(pClient->SectorLink));
	--(g_Sector[oldSectorPos.shY][oldSectorPos.shX].dwNumOfClient);
}

AroundInfo* GetAroundValidClient(SectorPos sp, st_Client* pExcept)
{
	SectorPos temp;
	int iNum = 0;
	for (int i = 0; i < 9; ++i)
	{
		temp.shY = sp.shY + spArrDir[i].shY;
		temp.shX = sp.shX + spArrDir[i].shX;
		if (IsValidSector(temp))
			GetValidClientFromSector(temp, &g_AroundInfo, &iNum, pExcept);
	}
	g_AroundInfo.CI.dwNum = iNum;
	return &g_AroundInfo;
}

// 락 거는 순서
// 영향권에서 없어진 섹터 -> 영향권에 들어오는 섹터 -> 섹터 옮기기위한 Exclusive 
// 락 푸는 순서
// 섹터 옮기기 위한 Exclusive -> 영향권에 들어오는 섹터 -> 영향권에서 없어진 섹터
void SectorUpdateAndNotify(st_Client* pClient, MOVE_DIR sectorMoveDir, SectorPos oldSectorPos, SectorPos newSectorPos, BOOL IsMove)
{
	st_SECTOR_AROUND removeSectorAround;
	MOVE_DIR oppositeDir = GetOppositeDir(sectorMoveDir);
	GetRemoveSector(oppositeDir, &removeSectorAround, oldSectorPos);

	st_SECTOR_AROUND newSectorAround;
	GetNewSector(sectorMoveDir, &newSectorAround, newSectorPos);

	LockInfo lockInfo;
	GetMoveLockInfo(&lockInfo, oldSectorPos, newSectorPos);

	// 락 걸기
	LockShared(&removeSectorAround);
	LockShared(&newSectorAround);

	AcquireSRWLockExclusive(&lockInfo.lockArr[0]);
	AcquireSRWLockExclusive(&lockInfo.lockArr[1]);

	for (BYTE i = 0; i < removeSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[removeSectorAround.Around[i].shY][removeSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			// 없어지는 애가 다른애들한테 보내기
			Packet* pSC_DELETE_CHARACTER_LEAVE_TO_ANCIENT = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pClient->dwID, pSC_DELETE_CHARACTER_LEAVE_TO_ANCIENT);
			st_Client* pRemovedClient = LinkToClient(pLink);

			AcquireSRWLockShared(&pRemovedClient->clientLock);
			g_GameServer.SendPacket(pRemovedClient->SessionId, pSC_DELETE_CHARACTER_LEAVE_TO_ANCIENT);
			// 그자리에 있던 애들이 없어지는 애한테 보내기
			Packet* pSC_DELETE_CHARACTER_ANCIENT_TO_LEAVE = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pRemovedClient->dwID, pSC_DELETE_CHARACTER_ANCIENT_TO_LEAVE);
			g_GameServer.SendPacket(pClient->SessionId, pSC_DELETE_CHARACTER_ANCIENT_TO_LEAVE);
			ReleaseSRWLockShared(&pRemovedClient->clientLock);
			pLink = pLink->pNext;
		}
	}

	for (BYTE i = 0; i < newSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[newSectorAround.Around[i].shY][newSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			// 생기는 애가 다른애들한테 나 생긴다고 보내기
			Packet* pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pClient->dwID, pClient->viewDir, pClient->pos, pClient->chHp, pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);

			st_Client* pAncientClient = LinkToClient(pLink);
			AcquireSRWLockShared(&pAncientClient->clientLock);
			g_GameServer.SendPacket(pAncientClient->SessionId, pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);
			if (IsMove)
			{
				Packet* pSC_MOVE_START_NEW_TO_ANCIENT = Packet::Alloc();
				MAKE_SC_MOVE_START(pClient->dwID, pClient->moveDir, pClient->pos, pSC_MOVE_START_NEW_TO_ANCIENT);
				g_GameServer.SendPacket(pAncientClient->SessionId, pSC_MOVE_START_NEW_TO_ANCIENT);
			}

			// 원시인 플레이어가 자기 생긴다고 새로 입장하는 애한테 보내기
			Packet* pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pAncientClient->dwID, pAncientClient->viewDir, pAncientClient->pos, pAncientClient->chHp, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);
			g_GameServer.SendPacket(pClient->SessionId, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);
			if (pAncientClient->moveDir != MOVE_DIR_NOMOVE)
			{
				Packet* pSC_MOVE_START_ANCIENT_TO_NEW = Packet::Alloc();
				MAKE_SC_MOVE_START(pAncientClient->dwID, pAncientClient->moveDir, pAncientClient->pos, pSC_MOVE_START_ANCIENT_TO_NEW);
				g_GameServer.SendPacket(pAncientClient->SessionId, pSC_MOVE_START_ANCIENT_TO_NEW);
			}
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pAncientClient->clientLock);
		}
	}
	// 섹터 리스트에 갱신
	RemoveClientAtSector(pClient, oldSectorPos);
	AddClientAtSector(pClient, newSectorPos);

	ReleaseSRWLockExclusive(&lockInfo.lockArr[1]);
	ReleaseSRWLockExclusive(&lockInfo.lockArr[0]);

	UnLockShared(&newSectorAround);
	UnLockShared(&removeSectorAround);
}

void GetSectorAround(st_SECTOR_AROUND* pOutSectorAround, SectorPos CurSector)
{
	// posX, posY에 pos의 좌표값 담기
	__m128i posY = _mm_set1_epi16(CurSector.shY);
	__m128i posX = _mm_set1_epi16(CurSector.shX);

	// 8방향 방향벡터 X성분 Y성분 준비
	__m128i DirVY = _mm_set_epi16(-1, -1, -1, +0, +0, +1, +1, +1);
	__m128i DirVX = _mm_set_epi16(-1, +0, +1, -1, +1, -1, +0, +1);

	// 더한다
	posY = _mm_add_epi16(posY, DirVY);
	posX = _mm_add_epi16(posX, DirVX);

	__m128i min = _mm_set1_epi16(-1);
	__m128i max = _mm_set1_epi16(df_SECTOR_HEIGHT);

	// PosX > min ? 0xFFFF : 0
	__m128i cmpMin = _mm_cmpgt_epi16(posX, min);
	// PosX < max ? 0xFFFF : 0
	__m128i cmpMax = _mm_cmplt_epi16(posX, max);
	__m128i resultX = _mm_and_si128(cmpMin, cmpMax);

	SHORT X[8];
	_mm_storeu_si128((__m128i*)X, posX);

	SHORT Y[8];
	cmpMin = _mm_cmpgt_epi16(posY, min);
	cmpMax = _mm_cmplt_epi16(posY, max);
	__m128i resultY = _mm_and_si128(cmpMin, cmpMax);
	_mm_storeu_si128((__m128i*)Y, posY);

	// _mm128i min은 더이상 쓸일이 없으므로 재활용한다.
	min = _mm_and_si128(resultX, resultY);

	SHORT ret[8];
	_mm_storeu_si128((__m128i*)ret, min);

	BYTE byCnt = 0;
	for (int i = 0; i < 4; ++i)
	{
		if (ret[i] == 0xFFFF)
		{
			pOutSectorAround->Around[byCnt].shY = Y[i];
			pOutSectorAround->Around[byCnt].shX = X[i];
			++byCnt;
		}
	}

	pOutSectorAround->Around[byCnt].shY = CurSector.shY;
	pOutSectorAround->Around[byCnt].shX = CurSector.shX;
	++byCnt;

	for (int i = 4; i < 8; ++i)
	{
		if (ret[i] == 0xFFFF)
		{
			pOutSectorAround->Around[byCnt].shY = Y[i];
			pOutSectorAround->Around[byCnt].shX = X[i];
			++byCnt;
		}
	}
	pOutSectorAround->byCount = byCnt;
}

#pragma warning(disable : 6001)
void GetNewSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, SectorPos nextSector)
{
	MOVE_DIR sectorPosArr[5];
	BYTE byCnt = 0;
	BYTE bySectorPosCnt = 0;
	switch (dir)
	{
	case MOVE_DIR_LL:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_LL;
		sectorPosArr[2] = MOVE_DIR_LD;
		byCnt = 3;
		break;
	case MOVE_DIR_LU:	
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_UU;
		sectorPosArr[2] = MOVE_DIR_RU;
		sectorPosArr[3] = MOVE_DIR_LL;
		sectorPosArr[4] = MOVE_DIR_LD;
		byCnt = 5;
		break;
	case MOVE_DIR_UU:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_UU;
		sectorPosArr[2] = MOVE_DIR_RU;
		byCnt = 3;
		break;
	case MOVE_DIR_RU:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_UU;
		sectorPosArr[2] = MOVE_DIR_RU;
		sectorPosArr[3] = MOVE_DIR_RR;
		sectorPosArr[4] = MOVE_DIR_RD;
		byCnt = 5;
		break;
	case MOVE_DIR_RR:
		sectorPosArr[0] = MOVE_DIR_RU;
		sectorPosArr[1] = MOVE_DIR_RR;
		sectorPosArr[2] = MOVE_DIR_RD;
		byCnt = 3;
		break;
	case MOVE_DIR_RD:
		sectorPosArr[0] = MOVE_DIR_RU;
		sectorPosArr[1] = MOVE_DIR_RR;
		sectorPosArr[2] = MOVE_DIR_LD;
		sectorPosArr[3] = MOVE_DIR_DD;
		sectorPosArr[4] = MOVE_DIR_RD;
		byCnt = 5;
		break;
	case MOVE_DIR_DD:
		sectorPosArr[0] = MOVE_DIR_LD;
		sectorPosArr[1] = MOVE_DIR_DD;
		sectorPosArr[2] = MOVE_DIR_RD;
		byCnt = 3;
		break;
	case MOVE_DIR_LD:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_LL;
		sectorPosArr[2] = MOVE_DIR_LD;
		sectorPosArr[3] = MOVE_DIR_DD;
		sectorPosArr[4] = MOVE_DIR_RD;
		byCnt = 5;
		break;
	case MOVE_DIR_NOMOVE:
		__debugbreak();
		break;
	default:
		__debugbreak();
		break;
	}

	for (BYTE i = 0; i < byCnt; ++i)
	{
		SectorPos tempSector;
		tempSector.shY = nextSector.shY + vArr[sectorPosArr[i]].shY;
		tempSector.shX = nextSector.shX + vArr[sectorPosArr[i]].shX;
		if (IsValidSector(tempSector))
		{
			pOutSectorAround->Around[bySectorPosCnt] = tempSector;
			++bySectorPosCnt;
		}
	}
	pOutSectorAround->byCount = bySectorPosCnt;
}

void GetRemoveSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, SectorPos prevSector)
{
	MOVE_DIR sectorPosArr[5];
	BYTE byCnt = 0;
	BYTE bySectorPosCnt = 0;
	switch (dir)
	{
	case MOVE_DIR_LL:
		sectorPosArr[0] = MOVE_DIR_RU;
		sectorPosArr[1] = MOVE_DIR_RR;
		sectorPosArr[2] = MOVE_DIR_RD;
		byCnt = 3;
		break;
	case MOVE_DIR_LU:
		sectorPosArr[0] = MOVE_DIR_RU;
		sectorPosArr[1] = MOVE_DIR_RR;
		sectorPosArr[2] = MOVE_DIR_LD;
		sectorPosArr[3] = MOVE_DIR_LL;
		sectorPosArr[4] = MOVE_DIR_RD;
		byCnt = 5;
		break;
	case MOVE_DIR_UU:
		sectorPosArr[0] = MOVE_DIR_LD;
		sectorPosArr[1] = MOVE_DIR_DD;
		sectorPosArr[2] = MOVE_DIR_RD;
		byCnt = 3;
		break;
	case MOVE_DIR_RU:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_LL;
		sectorPosArr[2] = MOVE_DIR_LD;
		sectorPosArr[3] = MOVE_DIR_DD;
		sectorPosArr[4] = MOVE_DIR_RD;
		byCnt = 5;
		break;
	case MOVE_DIR_RR:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_LL;
		sectorPosArr[2] = MOVE_DIR_LD;
		byCnt = 3;
		break;
	case MOVE_DIR_RD:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_UU;
		sectorPosArr[2] = MOVE_DIR_RU;
		sectorPosArr[3] = MOVE_DIR_LL;
		sectorPosArr[4] = MOVE_DIR_LD;
		byCnt = 5;
		break;
	case MOVE_DIR_DD:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_UU;
		sectorPosArr[2] = MOVE_DIR_RU;
		byCnt = 3;
		break;
	case MOVE_DIR_LD:
		sectorPosArr[0] = MOVE_DIR_LU;
		sectorPosArr[1] = MOVE_DIR_UU;
		sectorPosArr[2] = MOVE_DIR_RU;
		sectorPosArr[3] = MOVE_DIR_RR;
		sectorPosArr[4] = MOVE_DIR_RD;
		byCnt = 5;
		break;
	case MOVE_DIR_NOMOVE:
		__debugbreak();
		break;
	default:
		__debugbreak();
		break;
	}

	for (BYTE i = 0; i < byCnt; ++i)
	{
		SectorPos tempSector;
		tempSector.shY = prevSector.shY + vArr[sectorPosArr[i]].shY;
		tempSector.shX = prevSector.shX + vArr[sectorPosArr[i]].shX;
		if (IsValidSector(tempSector))
		{
			pOutSectorAround->Around[bySectorPosCnt] = tempSector;
			++bySectorPosCnt;
		}
	}
	pOutSectorAround->byCount = bySectorPosCnt;
}
#pragma warning(default : 6001)

