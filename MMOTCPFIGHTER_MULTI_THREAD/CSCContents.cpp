#include <WinSock2.h>
#include <windows.h>
#include "Direction.h"
#include "CSCContents.h"
#include "SCCContents.h"
#include "Sector.h"
#include "Stack.h"
#include "GameServer.h"
extern GameServer g_GameServer;

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

__forceinline void SyncProc(st_Client* pSyncClient, SectorPos recvSector)
{
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, recvSector);
	LockShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pSyncPacket = Packet::Alloc();
			MAKE_SC_SYNC(pSyncClient->dwID, pSyncClient->pos, pSyncPacket);
			g_GameServer.SendPacket(pOtherClient->SessionId, pSyncPacket);
			pLink = pLink->pNext;
		}
	}
	UnLockShared(&sectorAround);
}

void FindSectorInAttackRange(Pos pos, MOVE_DIR viewDir, SHORT shAttackRangeY, SHORT shAttackRangeX, st_SECTOR_AROUND* pSectorAround)
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
	pSectorAround->byCount = iCount;
}

st_Client* HandleCollision(st_Client* pAttacker, SHORT shAttackRangeY, SHORT shAttackRangeX, st_SECTOR_AROUND* pSectorAround)
{
	st_Client* pVictim = nullptr;

	if (pAttacker->viewDir == MOVE_DIR_LL)
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
			{
				st_Client* pOtherClient = LinkToClient(pLink);

				// 공격자 본인은 빼고 보내야한다
				if (pOtherClient == pAttacker)
					continue;

				AcquireSRWLockShared(&pOtherClient->clientLock);
				if (IsLLColide(pAttacker->pos, pOtherClient->pos, shAttackRangeY, shAttackRangeX))
				{
					pVictim = pOtherClient;
					ReleaseSRWLockShared(&pOtherClient->clientLock);
					return pVictim;
				}
				ReleaseSRWLockShared(&pOtherClient->clientLock);
				pLink = pLink->pNext;
			}
		}
	}
	else
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
			{
				st_Client* pOtherClient = LinkToClient(pLink);

				// 공격자 본인은 빼고 보내야한다
				if (pOtherClient == pAttacker)
					continue;

				AcquireSRWLockShared(&pOtherClient->clientLock);
				if (IsRRColide(pAttacker->pos, pOtherClient->pos, shAttackRangeY, shAttackRangeX))
				{
					pVictim = pOtherClient;
					ReleaseSRWLockShared(&pOtherClient->clientLock);
					return pVictim;
				}
				ReleaseSRWLockShared(&pOtherClient->clientLock);
				pLink = pLink->pNext;
			}
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(st_Client* pClient, MOVE_DIR moveDir, Pos clientPos)
{
	AcquireSRWLockExclusive(&pClient->clientLock);
	SectorPos oldSector = CalcSector(pClient->pos);

	// 클라이언트 시선처리
	switch (moveDir)
	{
	case MOVE_DIR_RU:
	case MOVE_DIR_RR:
	case MOVE_DIR_RD:
		pClient->viewDir = MOVE_DIR_RR;
		break;
	case MOVE_DIR_LL:
	case MOVE_DIR_LU:
	case MOVE_DIR_LD:
		pClient->viewDir = MOVE_DIR_LL;
		break;
	}

	// 이동중에 방향을 바꿔서 STOP이 오지않고 또 다시 START가 왓는데, 이때 서버 프레임이 떨어져서 싱크가 발생하는 경우.
	// 기존 서버 좌표 기준으로 근방 섹터에 SYNC메시지를 날려야하며 따라서 그 서버좌표 근방섹터에 전부 락을잡는다
	if (IsSync(pClient->pos, clientPos))
	{
		SyncProc(pClient, oldSector);
		clientPos = pClient->pos;
	}
	else
	{
		// 싱크가 아니므로 이제부터 클라 좌표를 믿는다
		pClient->pos = clientPos;
	}


	SectorPos newSector = CalcSector(clientPos);

	// IsSync가 참이엇다면 이시점에서 oldSector == newSector라서 if문 안거침
	// dfERROR_RANGE보다 오차가 작지만, 섹터가 틀려서 섹터를 클라기준으로 맞출경우 업데이트 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// 어차피 곧 움직인다고 보낼거기에 지금은 움직인다고 알릴필요 없다.
		SectorUpdateAndNotify(pClient, bySectorMoveDir, oldSector, newSector, FALSE);
	}

	// 같은 섹터라면 현재 위치한 섹터에 내가 움직이기 시작한다고 패킷을 보내야한다.
	// 근방섹터에 플레이어가 추가 혹은 삭제되는것이 아니기에 9개 섹터에 SHARED로 락을 건다
	// 전부 SC_MOVE_START 패킷 쏜 뒤 락풀고 리턴
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	LockShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pMoveStartPacket = Packet::Alloc();
			MAKE_SC_MOVE_START(pClient->dwID, pClient->moveDir, clientPos, pMoveStartPacket);
			g_GameServer.SendPacket(pClient->SessionId, pMoveStartPacket);
			pLink = pLink->pNext;
		}
	}
	UnLockShared(&sectorAround);
	ReleaseSRWLockExclusive(&pClient->clientLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(st_Client* pClient, MOVE_DIR viewDir, Pos clientPos)
{
	AcquireSRWLockExclusive(&pClient->clientLock);
	SectorPos oldSector = CalcSector(pClient->pos);

	// START이후 시간이 흘러 Stop이 왓는데, 그 사이에 서버 프레임이 떨어져서 싱크가 발생하는 경우.
	if (IsSync(pClient->pos, clientPos))
	{
		SyncProc(pClient, oldSector);
		clientPos = pClient->pos;
	}
	else
	{
		// 싱크가 아니므로 이제부터 클라 좌표를 믿는다
		pClient->pos = clientPos;
	}

	SectorPos newSector = CalcSector(clientPos);

	// IsSync가 참이엇다면 이시점에서 oldSector == newSector라서 if문 안거침
	// dfERROR_RANGE보다 오차가 작지만, 섹터가 틀려서 섹터를 클라기준으로 맞출경우 업데이트 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// 아직 움직이기 때문에 새로 보이는 애들한테 움직인다고 보내야 아래에서 다시 Stop 패킷을 보내는게 말이된다
		SectorUpdateAndNotify(pClient, bySectorMoveDir, oldSector, newSector, TRUE);
	}

	// 같은 섹터라면 현재 위치한 섹터에 내가 움직이기 시작한다고 패킷을 보내야한다.
	// 근방섹터에 플레이어가 추가 혹은 삭제되는것이 아니기에 9개 섹터에 SHARED로 락을 건다
	// 전부 SC_MOVE_STOP 패킷 쏜 뒤 락풀고 리턴
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	LockShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pMoveStopPacket = Packet::Alloc();
			MAKE_SC_MOVE_STOP(pClient->dwID, pClient->viewDir, clientPos, pMoveStopPacket);
			g_GameServer.SendPacket(pClient->SessionId, pMoveStopPacket);
			pLink = pLink->pNext;
		}
	}
	UnLockShared(&sectorAround);
	ReleaseSRWLockExclusive(&pClient->clientLock);
    return TRUE;
}

__forceinline void DamageProc(st_Client* pAttacker, st_Client* pVictim, st_SECTOR_AROUND* pSectorAround)
{
	for (BYTE i = 0; i < pSectorAround->byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pDamagePacket = Packet::Alloc();
			MAKE_SC_DAMAGE(pAttacker->dwID, pVictim->dwID, pVictim->chHp, pDamagePacket);

			AcquireSRWLockShared(&pOtherClient->clientLock);
			g_GameServer.SendPacket(pOtherClient->SessionId, pDamagePacket);
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pOtherClient->clientLock);
		}
	}
}

BOOL CS_ATTACK1(st_Client* pClient, MOVE_DIR viewDir, Pos clientPos)
{
	AcquireSRWLockShared(&pClient->clientLock);

	SectorPos curSector = CalcSector(clientPos);
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, curSector);
	LockShared(&sectorAround);

	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			st_Client* pOtherClient = LinkToClient(pLink);
			Packet* pAttack1Packet = Packet::Alloc();

			// 공격자 본인은 빼고 보내야한다
			if (pOtherClient == pClient)
				continue;

			MAKE_SC_ATTACK1(pClient->dwID, pClient->viewDir, pClient->pos, pAttack1Packet);
			AcquireSRWLockShared(&pOtherClient->clientLock);
			g_GameServer.SendPacket(pOtherClient->SessionId, pAttack1Packet);
			pLink = pLink->pNext;
			ReleaseSRWLockShared(&pOtherClient->clientLock);
		}
	}

	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(clientPos, viewDir, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision(pClient, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);

	if (!pVictim)
	{
		UnLockShared(&sectorAround);
		ReleaseSRWLockShared(&pClient->clientLock);
		return FALSE;
	}

	pVictim->chHp -= dfATTACK1_DAMAGE;
	DamageProc(pClient, pVictim, &sectorAround);

	UnLockShared(&sectorAround);
	ReleaseSRWLockShared(&pClient->clientLock);
	return TRUE;
}

BOOL CS_ATTACK2(st_Client* pClient, MOVE_DIR viewDir, Pos ClientPos)
{
	AcquireSRWLockShared(&pClient->clientLock);
	ReleaseSRWLockShared(&pClient->clientLock);
	return TRUE;
}

