#include <WinSock2.h>
#include <emmintrin.h>
#include "Sector.h"
#include "Packet.h"
#include "SCCContents.h"
#include "Stack.h"
#include "IHandler.h"
#include "GameServer.h"
#include "Update.h"
#include "MemLog.h"

extern GameServer g_GameServer;

st_SECTOR_CLIENT_INFO g_Sector[dwNumOfSectorVertical][dwNumOfSectorHorizon];

int g_iCounter;
MemLog g_logArr[1001000];


void GetMoveLockInfo(SECTOR_AROUND* pMoveSectorAround, SectorPos prevSector, SectorPos afterSector)
{
	pMoveSectorAround->iCnt = 2;
	if (prevSector.shY < afterSector.shY)
	{
		pMoveSectorAround->Around[0] = prevSector;
		pMoveSectorAround->Around[1] = afterSector;
		return;
	}

	if (prevSector.shY > afterSector.shY)
	{
		pMoveSectorAround->Around[0] = afterSector;
		pMoveSectorAround->Around[1] = prevSector;
		return;
	}

	// Y�� ������ X���� ���ؼ� ����, X�� ū���� �� �ڿ� ��
	if (prevSector.shX < afterSector.shX)
	{
		pMoveSectorAround->Around[0] = prevSector;
		pMoveSectorAround->Around[1] = afterSector;
	}
	else
	{
		pMoveSectorAround->Around[0] = afterSector;
		pMoveSectorAround->Around[1] = prevSector;
	}
}

void GetMoveSectorInfo(MOVE_SECTOR_INFO* pOutMSI, SECTOR_AROUND* pMoveSectorAround, SECTOR_AROUND* pRemoveSectorAround, SECTOR_AROUND* pNewSectorAround)
{
	int iCnt = 0;
	for (int i = 0; i < pRemoveSectorAround->iCnt; ++i)
	{
		pOutMSI->spArr[iCnt] = pRemoveSectorAround->Around[i];
		pOutMSI->bExclusive[iCnt++] = FALSE;
	}

	for (int i = 0; i < pNewSectorAround->iCnt; ++i)
	{
		pOutMSI->spArr[iCnt] = pNewSectorAround->Around[i];
		pOutMSI->bExclusive[iCnt++] = FALSE;
	}

	for (int i = 0; i < pMoveSectorAround->iCnt; ++i)
	{
		pOutMSI->spArr[iCnt] = pMoveSectorAround->Around[i];
		pOutMSI->bExclusive[iCnt++] = TRUE;
	}

	pOutMSI->iCnt = iCnt;
}

__forceinline void ReleaseFailMoveLocks(MOVE_SECTOR_INFO* pMSI, int idx)
{
	for (int j = idx - 1; j >= 0; --j)
	{
		if (pMSI->bExclusive[j])
			ReleaseSRWLockExclusive(&g_Sector[pMSI->spArr[j].shY][pMSI->spArr[j].shX].srwSectionLock);
		else
			ReleaseSRWLockShared(&g_Sector[pMSI->spArr[j].shY][pMSI->spArr[j].shX].srwSectionLock);
	}
}

__forceinline BOOL TryAcquireMoveLock(MOVE_SECTOR_INFO* pMSI)
{
	for(int i = 0; i < pMSI->iCnt; ++i)
	{
		if (pMSI->bExclusive[i])
		{
			if (!TryAcquireSRWLockExclusive(&g_Sector[pMSI->spArr[i].shY][pMSI->spArr[i].shX].srwSectionLock))
			{
				ReleaseFailMoveLocks(pMSI, i);
				return FALSE;
			}
		}
		else
		{
			if (!TryAcquireSRWLockShared(&g_Sector[pMSI->spArr[i].shY][pMSI->spArr[i].shX].srwSectionLock))
			{
				ReleaseFailMoveLocks(pMSI, i);
				return FALSE;
			}
		}
	}
	return TRUE;
}

__forceinline void ReleaseMoveLock(MOVE_SECTOR_INFO* pMSI)
{
	for (int i = pMSI->iCnt - 1; i >= 0; --i)
	{
		if (pMSI->bExclusive[i])
			ReleaseSRWLockExclusive(&g_Sector[pMSI->spArr[i].shY][pMSI->spArr[i].shX].srwSectionLock);
		else
			ReleaseSRWLockShared(&g_Sector[pMSI->spArr[i].shY][pMSI->spArr[i].shX].srwSectionLock);
	}
}

void InitSectionLock()
{
	for (DWORD i = 0; i < dwNumOfSectorVertical; ++i)
	{
		for (DWORD j = 0; j < dwNumOfSectorHorizon; ++j)
		{
			InitializeSRWLock(&g_Sector[i][j].srwSectionLock);
		}
	}
}

void AddClientAtSector(Player* pClient, SectorPos newSectorPos)
{
	LinkToLinkedListLast(&(g_Sector[newSectorPos.shY][newSectorPos.shX].pClientLinkHead), &(g_Sector[newSectorPos.shY][newSectorPos.shX].pClientLinkTail), &(pClient->SectorLink));
	++(g_Sector[newSectorPos.shY][newSectorPos.shX].iNumOfClient);
}

void RemoveClientAtSector(Player* pClient, SectorPos oldSectorPos)
{
	UnLinkFromLinkedList(&(g_Sector[oldSectorPos.shY][oldSectorPos.shX].pClientLinkHead), &(g_Sector[oldSectorPos.shY][oldSectorPos.shX].pClientLinkTail), &(pClient->SectorLink));
	--(g_Sector[oldSectorPos.shY][oldSectorPos.shX].iNumOfClient);
}

// �� �Ŵ� ����
// ����ǿ��� ������ ���� -> ����ǿ� ������ ���� -> ���� �ű������ Exclusive 
// �� Ǫ�� ����
// ���� �ű�� ���� Exclusive -> ����ǿ� ������ ���� -> ����ǿ��� ������ ����
void SectorUpdateAndNotify(Player* pPlayer, MOVE_DIR sectorMoveDir, SectorPos oldSectorPos, SectorPos newSectorPos, BOOL IsMove, BOOL IsUpdate)
{
	SECTOR_AROUND removeSectorAround;
	GetRemoveSector(sectorMoveDir, &removeSectorAround, oldSectorPos);

	SECTOR_AROUND newSectorAround;
	GetNewSector(sectorMoveDir, &newSectorAround, newSectorPos);

	SECTOR_AROUND moveSectorAround;
	GetMoveLockInfo(&moveSectorAround, oldSectorPos, newSectorPos);

	// �� �ɱ�
	MOVE_SECTOR_INFO moveSectorInfo;
	GetMoveSectorInfo(&moveSectorInfo, &moveSectorAround, &removeSectorAround, &newSectorAround);

	// ���ŵǴ� ����, ���λ���� ���Ϳ� S��, �÷��̾ ���� ���͸� �ű�� ����  E���� �Ǵ�. Update ,Start ,Stop��� Player�� E���� �����ϰ�, �̶� �̵��� ���� ���Ϳ� E���� �ɸ� ����ϰ�
	// �ٸ� ��������� �ش� �÷��̾�� �޽����� ������ ���� S���� �ɰų� ��Ÿ ������ E���� �� ��(���ٿ����� E���� ����ϰ� �� ����), ������ �÷��̾�� �޽����� ������ ���� S���� �ɾ� ������� �߻��Ѵ�
	// �̸� �����ϱ� ���� �÷��̾ E���� ������ �������� ��� S���� �Ǿ��� E���� �Ǿ��� ������ �� ������ �����ϸ� �÷��̾� E������ ��� Ǯ������� �����̸� ���µ� �̶� Player�� Release���� �ʰ��ϱ� ���� 
	// START�� Stop���� �°�� MoveLockȹ����н� �÷��̾� E�� �����ϰ� �ٽ� �Ŵ»����� Release�� �������� srwPlayerArrLock�� S���� �Ǵ�
	//if (!IsUpdate)
	//{
	//	AcquireSRWLockShared(&g_srwPlayerArrLock);
	//	WRITE_MEMORY_LOG(SECTOR_UPDATE_AND_NOTIFY,SHARED,ACQUIRE);
	//}
	do
	{
		if (TryAcquireMoveLock(&moveSectorInfo))
		{
			//if (!IsUpdate) {
			//	ReleaseSRWLockShared(&g_srwPlayerArrLock);
			//	WRITE_MEMORY_LOG(SECTOR_UPDATE_AND_NOTIFY, SHARED, RELEASE);
			//}

			break;
		}

		ReleaseSRWLockExclusive(&pPlayer->playerLock);
		AcquireSRWLockExclusive(&pPlayer->playerLock);
	} while (true);

	// �þ߿��� �������� ���Ϳ��� pPlayer�� ���Ե��� �ʱ⿡ pAncient == pPlayer�� ���� ����ó���� ���� �ʾƵ� �ȴ�.
	for (int i = 0; i < removeSectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[removeSectorAround.Around[i].shY][removeSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAncient = LinkToClient(pLink);

			// ������ �ְ� ���ֹε����� ������
			Packet* pSC_DELETE_CHARACTER_LEAVE_TO_ANCIENT = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pPlayer->dwID, pSC_DELETE_CHARACTER_LEAVE_TO_ANCIENT);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_DELETE_CHARACTER_LEAVE_TO_ANCIENT);

			// ���ֹε��� ������ ������ ������
			Packet* pSC_DELETE_CHARACTER_ANCIENT_TO_LEAVE = Packet::Alloc();
			MAKE_SC_DELETE_CHARACTER(pAncient->dwID, pSC_DELETE_CHARACTER_ANCIENT_TO_LEAVE);
			g_GameServer.SendPacket(pPlayer->SessionId, pSC_DELETE_CHARACTER_ANCIENT_TO_LEAVE);
			pLink = pLink->pNext;
		}
	}

	// �þ߿��� ������ ���Ϳ��� pPlayer�� ���Ե��� �ʱ⿡ pAncient == pPlayer�� ���� ����ó���� ���� �ʾƵ� �ȴ�.
	for (int i = 0; i < newSectorAround.iCnt; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[newSectorAround.Around[i].shY][newSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (int j = 0; j < pSCI->iNumOfClient; ++j)
		{
			Player* pAncient = LinkToClient(pLink);

			// ���� ���ֹ��� �����̰� �ִٸ� ��ġ�� �̵������� �ʿ��ϰ� �̴� Update�����尡 ���� �����ϱ⿡ ���⼭�� ���� �Ǵ�
			AcquireSRWLockShared(&pAncient->playerLock);
			// ����� �ְ� �ٸ��ֵ����� �� ����ٰ� ������
			Packet* pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pPlayer->hp, pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_CREATE_OTHER_CHARACTER_NEW_TO_ANCIENT);
			if (IsMove)
			{
				Packet* pSC_MOVE_START_NEW_TO_ANCIENT = Packet::Alloc();
				MAKE_SC_MOVE_START(pPlayer->dwID, pPlayer->moveDir, pPlayer->pos, pSC_MOVE_START_NEW_TO_ANCIENT);
				g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_START_NEW_TO_ANCIENT);
			}

			// ���ֹ��� ���� �����ϴ� ������ ������
			Packet* pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW = Packet::Alloc();
			MAKE_SC_CREATE_OTHER_CHARACTER(pAncient->dwID, pAncient->viewDir, pAncient->pos, pAncient->hp, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);
			g_GameServer.SendPacket(pPlayer->SessionId, pSC_CREATE_OTHER_CHARACTER_ANCIENT_TO_NEW);

			// �����δٸ� ���ֹ��� ���� �����ϴ� ������ �����δٰ� ������, �̰Ŷ����� ������ ���ֹ����� ���� �ǰ��̴�
			if (pAncient->moveDir != MOVE_DIR_NOMOVE)
			{
				Packet* pSC_MOVE_START_ANCIENT_TO_NEW = Packet::Alloc();
				MAKE_SC_MOVE_START(pAncient->dwID, pAncient->moveDir, pAncient->pos, pSC_MOVE_START_ANCIENT_TO_NEW);
				g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_START_ANCIENT_TO_NEW);
			}
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pAncient->playerLock);
		}
	}
	RemoveClientAtSector(pPlayer, oldSectorPos);
	AddClientAtSector(pPlayer, newSectorPos);

	ReleaseMoveLock(&moveSectorInfo);
}

void GetSectorAround(SECTOR_AROUND* pOutSectorAround, SectorPos CurSector)
{
	// posX, posY�� pos�� ��ǥ�� ���
	__m128i posY = _mm_set1_epi16(CurSector.shY);
	__m128i posX = _mm_set1_epi16(CurSector.shX);

	// 8���� ���⺤�� X���� Y���� �غ�
	__m128i DirVY = _mm_set_epi16(+1, +1, +1, +0, +0, -1, -1, -1);
	__m128i DirVX = _mm_set_epi16(+1, +0, -1, +1, -1, +1, +0, -1);

	// ���Ѵ�
	posY = _mm_add_epi16(posY, DirVY);
	posX = _mm_add_epi16(posX, DirVX);

	__m128i min = _mm_set1_epi16(-1);
	__m128i max = _mm_set1_epi16(dwNumOfSectorVertical);

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

	// _mm128i min�� ���̻� ������ �����Ƿ� ��Ȱ���Ѵ�.
	min = _mm_and_si128(resultX, resultY);

	SHORT ret[8];
	_mm_storeu_si128((__m128i*)ret, min);

	int iCnt = 0;
	for (int i = 0; i < 4; ++i)
	{
		if (ret[i] == (short)0xffff)
		{
			pOutSectorAround->Around[iCnt].shY = Y[i];
			pOutSectorAround->Around[iCnt].shX = X[i];
			++iCnt;
		}
	}

	pOutSectorAround->Around[iCnt].shY = CurSector.shY;
	pOutSectorAround->Around[iCnt].shX = CurSector.shX;
	++iCnt;

	for (int i = 4; i < 8; ++i)
	{
		if (ret[i] == (short)0xffff)
		{
			pOutSectorAround->Around[iCnt].shY = Y[i];
			pOutSectorAround->Around[iCnt].shX = X[i];
			++iCnt;
		}
	}
	pOutSectorAround->iCnt = iCnt;
}

#pragma warning(disable : 6001)
void GetNewSector(MOVE_DIR dir, SECTOR_AROUND* pOutSectorAround, SectorPos nextSector)
{
	MOVE_DIR sectorPosArr[5];
	BYTE byCnt = 0;
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

	BYTE bySectorPosCnt = 0;
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
	pOutSectorAround->iCnt = bySectorPosCnt;
}

void GetRemoveSector(MOVE_DIR dir, SECTOR_AROUND* pOutSectorAround, SectorPos prevSector)
{
	MOVE_DIR sectorPosArr[5];
	BYTE byCnt = 0;
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

	BYTE bySectorPosCnt = 0;
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
	pOutSectorAround->iCnt = bySectorPosCnt;
}
#pragma warning(default : 6001)

