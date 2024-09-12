#include <Windows.h>
#include "LinkedList.h"
#include "Constant.h"
#include "Position.h"
#include "Sector.h"
#include "Constant.h"
#include "Client.h"
#include "MemLog.h"

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

		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);

		// Update는 이미 자료구조에 대한 락을 들고잇기 때문에 뭘하던지 중간에 세션이 릴리즈 될 위험성 자체가 없음
		SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, TRUE);

		pPlayer->pos = newPos;
		ReleaseSRWLockExclusive(&pPlayer->playerLock);
	}
	ReleaseSRWLockShared(&g_srwPlayerArrLock);
}
