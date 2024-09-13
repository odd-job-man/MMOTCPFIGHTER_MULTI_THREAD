#include <Windows.h>
#include "LinkedList.h"
#include "Constant.h"
#include "Position.h"
#include "Sector.h"
#include "Constant.h"
#include "Client.h"
#include "stdio.h"

Player* g_playerArr[MAX_SESSION];
DWORD g_dwPlayerNum;
SRWLOCK g_srwPlayerArrLock;

void Update()
{
	AcquireSRWLockShared(&g_srwPlayerArrLock);
	for (DWORD i = 0; i < g_dwPlayerNum; ++i)
	{
		Player* pPlayer = g_playerArr[i];

		AcquireSRWLockExclusive(&pPlayer->playerLock);
		if (pPlayer->moveDir == MOVE_DIR_NOMOVE)
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			continue;
		}

		Pos oldPos = pPlayer->pos;
		Pos newPos;

		newPos.shY = oldPos.shY + vArr[pPlayer->moveDir].shY * dfSPEED_PLAYER_Y;
		newPos.shX = oldPos.shX + vArr[pPlayer->moveDir].shX * dfSPEED_PLAYER_X;

		if (!IsValidPos(newPos))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			continue;
		}

		SectorPos oldSector = CalcSector(oldPos);
		SectorPos newSector = CalcSector(newPos);

		if (IsSameSector(oldSector, newSector))
		{
			pPlayer->pos = newPos;
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			continue;
		}
		// Update는 이미 자료구조에 대한 락을 들고잇기 때문에 뭘하던지 중간에 세션이 릴리즈 될 위험성 자체가 없음
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		SECTOR_AROUND incomingSectorS;
		SECTOR_AROUND disappearingSectorS;
		SECTOR_AROUND moveSectorS;
		GetNewSector(sectorMoveDir, &incomingSectorS, newSector);
		GetRemoveSector(sectorMoveDir, &disappearingSectorS, oldSector);
		GetMoveSectorS(&moveSectorS, oldSector, newSector);

		Pos backUpPos = newPos;
		MOVE_SECTOR_INFO msi;
		GetMoveSectorInfo(&msi, &moveSectorS,&disappearingSectorS,&incomingSectorS);
		while (!TryAcquireMoveLock(&msi))
		{
			ReleaseSRWLockExclusive(&pPlayer->playerLock);
			AcquireSRWLockExclusive(&pPlayer->playerLock);
		}
		if (backUpPos.YX == newPos.YX)
		{
			BroadcastCreateAndDelete_ON_START_OR_STOP(pPlayer, &incomingSectorS, &disappearingSectorS, TRUE);
			RemoveClientAtSector(pPlayer, oldSector);
			AddClientAtSector(pPlayer, newSector);
			pPlayer->pos = newPos;
		}
		ReleaseMoveLock(&msi);
		ReleaseSRWLockExclusive(&pPlayer->playerLock);
	}
	ReleaseSRWLockShared(&g_srwPlayerArrLock);
}
