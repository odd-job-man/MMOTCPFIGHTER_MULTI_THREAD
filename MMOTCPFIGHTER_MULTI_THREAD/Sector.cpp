#include "Sector.h"
#include "Packet.h"
#include "SCCContents.h"
#include "Stack.h"
#include "GameServer.h"

extern GameServer g_GameServer;

AroundInfo g_AroundInfo;
st_SECTOR_CLIENT_INFO g_Sector[dwNumOfSectorVertical][dwNumOfSectorHorizon];

void LockAroundSectorShared(SectorPos sector)
{
	SectorPos sectorToLock;
	for (DWORD i = 0; i < 9; ++i)
	{
		sectorToLock.shX = sector.shX + spArrDir[i].shX;
		sectorToLock.shY = sector.shY + spArrDir[i].shY;
		if (IsValidSector(sectorToLock))
			AcquireSRWLockShared(&g_Sector[sectorToLock.shY][sectorToLock.shX].srwSectionLock);
	}
}
void UnLockAroundSectorShared(SectorPos sector)
{
	// Lock한거 역순으로 언락
	SectorPos sectorToUnlock;
	for (int i = 8; i >= 0; --i)
	{
		sectorToUnlock.shX = sector.shX + spArrDir[i].shX;
		sectorToUnlock.shY = sector.shY + spArrDir[i].shY;
		if (IsValidSector(sectorToUnlock))
			ReleaseSRWLockShared(&g_Sector[sectorToUnlock.shY][sectorToUnlock.shX].srwSectionLock);
	}
}


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

void SectorUpdateAndNotify(st_Client* pClient, MOVE_DIR sectorMoveDir, SectorPos oldSectorPos, SectorPos newSectorPos, BOOL IsMove)
{
	LockInfo lockInfo;
	GetMoveLockInfo(&lockInfo, oldSectorPos, newSectorPos);
	AcquireSRWLockExclusive(&lockInfo.lockArr[0]);
	AcquireSRWLockExclusive(&lockInfo.lockArr[1]);

	// 섹터 리스트에 갱신
	RemoveClientAtSector(pClient, oldSectorPos);
	AddClientAtSector(pClient, newSectorPos);

	ReleaseSRWLockExclusive(&lockInfo.lockArr[1]);
	ReleaseSRWLockExclusive(&lockInfo.lockArr[0]);

	// 영향권에서 없어진 섹터를 구함
	st_SECTOR_AROUND removeSectorAround;
	LockInfo removeLockInfo;
	MOVE_DIR oppositeDir = GetOppositeDir(sectorMoveDir);
	GetRemoveSector(oppositeDir, &removeSectorAround, &removeLockInfo, oldSectorPos);

	for (int i = 0; i < removeLockInfo.iLockNum; ++i)
		AcquireSRWLockShared(&removeLockInfo.lockArr[i]);

	for (BYTE i = 0; i < removeSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[removeSectorAround.Around[i].shY][removeSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			// 없어지는 애가 다른애들한테 보내기
			Packet* pDeletePacket1ToALL = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pClient->dwID, pDeletePacket1ToALL);
			st_Client* pRemovedClient = LinkToClient(pLink);
			g_GameServer.SendPacket(pRemovedClient->SessionId, pDeletePacket1ToALL);

			// 그자리에 있던 애들이 없어지는 애한테 보내기
			Packet* pDeletePacketALLTo1;
			MAKE_SC_DELETE_CHARACTER(pRemovedClient->dwID, pDeletePacketALLTo1);
			g_GameServer.SendPacket(pClient->SessionId, pDeletePacketALLTo1);
		}
	}

	for (int i = removeLockInfo.iLockNum - 1; i >= 0; --i)
		ReleaseSRWLockShared(&removeLockInfo.lockArr[i]);


	// 영향권으로 들어오는 섹터를 구함
	st_SECTOR_AROUND newSectorAround;
	LockInfo newLockInfo;
	GetNewSector(sectorMoveDir, &newSectorAround, &newLockInfo, newSectorPos);

	for (int i = 0; i < newLockInfo.iLockNum; ++i)
		AcquireSRWLockShared(&newLockInfo.lockArr[i]);

	for (BYTE i = 0; i < newSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[newSectorAround.Around[i].shY][newSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			// 생기는 애가 다른애들한테 나 생긴다고 보내기
			Packet* pCreatePacket1ToALL = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pClient->dwID, pClient->viewDir, pClient->pos, pClient->chHp, pCreatePacket1ToALL);

			st_Client* pAncientClient = LinkToClient(pLink);
			g_GameServer.SendPacket(pAncientClient->SessionId, pCreatePacket1ToALL);
			if (IsMove)
			{
				Packet* pMovePacket1ToALL = Packet::Alloc();
				MAKE_SC_MOVE_START(pClient->dwID, pClient->moveDir, pClient->pos, pMovePacket1ToALL);
				g_GameServer.SendPacket(pAncientClient->SessionId, pMovePacket1ToALL);
			}

			// 원시인 플레이어가 자기 생긴다고 새로 입장하는 애한테 보내기
			Packet* pCreatePacketALLTo1 = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pAncientClient->dwID, pAncientClient->viewDir, pAncientClient->pos, pAncientClient->chHp, pCreatePacketALLTo1);
			g_GameServer.SendPacket(pClient->SessionId, pCreatePacketALLTo1);
			if (pAncientClient->moveDir != MOVE_DIR_NOMOVE)
			{
				Packet* pMovePacketALLTo1 = Packet::Alloc();
				MAKE_SC_MOVE_START(pAncientClient->dwID, pAncientClient->moveDir, pAncientClient->pos, pMovePacketALLTo1);
				g_GameServer.SendPacket(pAncientClient->SessionId, pMovePacketALLTo1);
			}
		}

	}

	for (int i = newLockInfo.iLockNum - 1; i >= 0; --i)
		ReleaseSRWLockShared(&newLockInfo.lockArr[i]);
}

void GetNewSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, LockInfo* pOutLockInfo, SectorPos nextSector)
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

#pragma warning(push : 6001)
	for (BYTE i = 0; i < byCnt; ++i)
	{
		SectorPos tempSector;
		tempSector.shY = nextSector.shY + vArr[sectorPosArr[i]].shY;
		tempSector.shX = nextSector.shX + vArr[sectorPosArr[i]].shX;
		if (IsValidSector(tempSector))
		{
			pOutSectorAround->Around[bySectorPosCnt] = tempSector;
			pOutLockInfo->lockArr[bySectorPosCnt] = g_Sector[tempSector.shY][tempSector.shX].srwSectionLock;
			++bySectorPosCnt;
		}
	}
	pOutSectorAround->byCount = bySectorPosCnt;
	pOutLockInfo->iLockNum = bySectorPosCnt;
#pragma warning(default : 6001)
}
void GetRemoveSector(MOVE_DIR dir, st_SECTOR_AROUND* pOutSectorAround, LockInfo* pOutLockInfo, SectorPos prevSector)
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

#pragma warning(push : 6001)
	for (BYTE i = 0; i < byCnt; ++i)
	{
		SectorPos tempSector;
		tempSector.shY = prevSector.shY + vArr[sectorPosArr[i]].shY;
		tempSector.shX = prevSector.shX + vArr[sectorPosArr[i]].shX;
		if (IsValidSector(tempSector))
		{
			pOutSectorAround->Around[bySectorPosCnt] = tempSector;
			pOutLockInfo->lockArr[bySectorPosCnt] = g_Sector[tempSector.shY][tempSector.shX].srwSectionLock;
			++bySectorPosCnt;
		}
	}
	pOutSectorAround->byCount = bySectorPosCnt;
	pOutLockInfo->iLockNum = bySectorPosCnt;
#pragma warning(default : 6001)
}

