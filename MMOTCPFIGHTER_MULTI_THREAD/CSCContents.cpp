#include <WinSock2.h>
#include <windows.h>
#include "Session.h"
#include "Direction.h"
#include "CSCContents.h"
#include "SCCContents.h"
#include "Sector.h"
#include "Stack.h"
#include "IHandler.h"
#include "GameServer.h"
#include <stdio.h>
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

__forceinline void SyncProc(st_Client* pSyncPlayer, SectorPos recvSector)
{
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, recvSector);
	AcquireSectorAroundShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD i = 0; i < pSCI->dwNumOfClient; ++i)
		{
			// 서버 기준으로 원래 거기잇던 원주민 and 싱크 당사자에게도 보내야하며 이 과정에서 SessionId만 필요해서 락을 걸지 않앗음
			st_Client* pAncient = LinkToClient(pLink);
			Packet* pSyncPacket = Packet::Alloc();
			MAKE_SC_SYNC(pSyncPlayer->dwID, pSyncPlayer->pos, pSyncPacket);
			g_GameServer.SendPacket(pAncient->SessionId, pSyncPacket);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
}

// 움직이기 시작한 플레이어 시선처리
__forceinline void ProcessPlayerViewDir(st_Client* pMoveStartPlayer, MOVE_DIR moveDir)
{
	switch (moveDir)
	{
	case MOVE_DIR_RU:
	case MOVE_DIR_RR:
	case MOVE_DIR_RD:
		pMoveStartPlayer->viewDir = MOVE_DIR_RR;
		break;
	case MOVE_DIR_LL:
	case MOVE_DIR_LU:
	case MOVE_DIR_LD:
		pMoveStartPlayer->viewDir = MOVE_DIR_LL;
		break;
	}
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

// 피격시 락을 풀지 않는 이유는 그 사이에 OnRelease로 인해서 없어지거나 혹은 그로인해 다른 클라로 바뀌는것을 막기 위함이다.
st_Client* HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(st_Client* pAttacker, SHORT shAttackRangeY, SHORT shAttackRangeX, st_SECTOR_AROUND* pSectorAround)
{
	if (pAttacker->viewDir == MOVE_DIR_LL)
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
			{
				st_Client* pHitCandidate = LinkToClient(pLink);

				if (pHitCandidate == pAttacker)
				{
					pLink = pLink->pNext;
					continue;
				}

				AcquireSRWLockExclusive(&pHitCandidate->playerLock);
				if (IsLLColide(pAttacker->pos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
					return pHitCandidate;

				pLink = pLink->pNext;
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
			}
		}
	}
	else
	{
		for (int i = 0; i < pSectorAround->byCount; ++i)
		{
			st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[pSectorAround->Around[i].shY][pSectorAround->Around[i].shX];
			LINKED_NODE* pLink = pSCI->pClientLinkHead;
			for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
			{
				st_Client* pHitCandidate = LinkToClient(pLink);

				// 공격자 본인은 빼고 보내야한다
				if (pHitCandidate == pAttacker)
				{
					pLink = pLink->pNext;
					continue;
				}

				AcquireSRWLockExclusive(&pHitCandidate->playerLock);
				if (IsRRColide(pAttacker->pos, pHitCandidate->pos, shAttackRangeY, shAttackRangeX))
					return pHitCandidate;

				pLink = pLink->pNext;
				ReleaseSRWLockExclusive(&pHitCandidate->playerLock);
			}
		}
	}
	return nullptr;
}

BOOL CS_MOVE_START(st_Client* pPlayer, MOVE_DIR moveDir, Pos clientPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	//printf("CS_MOVE_START! ThreadID : %u (%d, %d)\n", GetCurrentThreadId(), clientPos.shX, clientPos.shY);
	SectorPos oldSector = CalcSector(pPlayer->pos);

	// 클라이언트 시선처리
	ProcessPlayerViewDir(pPlayer, moveDir);
	pPlayer->moveDir = moveDir;

	// 이동중에 방향을 바꿔서 STOP이 오지않고 또 다시 START가 왓는데, 이때 서버 프레임이 떨어져서 싱크가 발생하는 경우.
	// 기존 서버 좌표 기준으로 근방 섹터에 SYNC메시지를 날려야하며 따라서 그 서버좌표 근방섹터에 전부 락을잡는다
	if (IsSync(pPlayer->pos, clientPos))
	{
		//printf("Sync! ThreadID : %u\n", GetCurrentThreadId());
		SyncProc(pPlayer, oldSector);
		clientPos = pPlayer->pos;
	}
	else
	{
		// 싱크가 아니므로 이제부터 클라 좌표를 믿는다
		pPlayer->pos = clientPos;
	}


	SectorPos newSector = CalcSector(clientPos);

	// IsSync가 참이엇다면 이시점에서 oldSector == newSector라서 if문 안거침
	// dfERROR_RANGE보다 오차가 작지만, 섹터가 틀려서 섹터를 클라기준으로 맞출경우 업데이트 
	if (!IsSameSector(oldSector, newSector))
	{
		//printf("CS_MOVE_START! ThreadID : %u SectorMOVE (%d,%d) -> (%d,%d)\n", GetCurrentThreadId(), oldSector.shX, oldSector.shY, newSector.shX, newSector.shY);
		MOVE_DIR sectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// 어차피 곧 움직인다고 보낼거기에 지금은 움직인다고 알릴필요 없다.
		SectorUpdateAndNotify(pPlayer, sectorMoveDir, oldSector, newSector, FALSE);
	}

	// 같은 섹터라면 현재 위치한 섹터에 내가 움직이기 시작한다고 패킷을 보내야한다.
	// 근방섹터에 플레이어가 추가 혹은 삭제되는것이 아니기에 9개 섹터에 SHARED로 락을 건다
	// 전부 SC_MOVE_START 패킷 쏜 뒤 락풀고 리턴
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	AcquireSectorAroundShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAncient = LinkToClient(pLink);
			// 나 자신에게는 START를 안보내야함, 이거 if문 위치때문에 메모리 릭 날뻔함
			if (pAncient == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			Packet* pSC_MOVE_START = Packet::Alloc();
			MAKE_SC_MOVE_START(pPlayer->dwID, pPlayer->moveDir, clientPos, pSC_MOVE_START);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_START);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
    return TRUE;
}

BOOL CS_MOVE_STOP(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockExclusive(&pPlayer->playerLock);
	//printf("CS_MOVE_STOP! ThreadID : %u (%d, %d)\n", GetCurrentThreadId(), playerPos.shX, playerPos.shY);
	SectorPos oldSector = CalcSector(pPlayer->pos);
	pPlayer->moveDir = MOVE_DIR_NOMOVE;

	// START이후 시간이 흘러 Stop이 왓는데, 그 사이에 서버 프레임이 떨어져서 싱크가 발생하는 경우.
	if (IsSync(pPlayer->pos, playerPos))
	{
		SyncProc(pPlayer, oldSector);
		playerPos = pPlayer->pos;
	}
	else
	{
		// 싱크가 아니므로 이제부터 클라 좌표를 믿는다
		pPlayer->pos = playerPos;
	}

	SectorPos newSector = CalcSector(playerPos);

	// IsSync가 참이엇다면 이시점에서 oldSector == newSector라서 if문 안거침
	// dfERROR_RANGE보다 오차가 작지만, 섹터가 틀려서 섹터를 클라기준으로 맞출경우 업데이트 
	if (!IsSameSector(oldSector, newSector))
	{
		//printf("CS_MOVE_STOP! ThreadID : %u SectorMOVE (%d,%d) -> (%d,%d)\n", GetCurrentThreadId(), oldSector.shX, oldSector.shY, newSector.shX, newSector.shY);
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// 아직 움직이기 때문에 새로 보이는 애들한테 움직인다고 보내야 아래에서 다시 Stop 패킷을 보내는게 말이된다
		SectorUpdateAndNotify(pPlayer, bySectorMoveDir, oldSector, newSector, TRUE);
	}

	// 같은 섹터라면 현재 위치한 섹터에 내가 움직이기 시작한다고 패킷을 보내야한다.
	// 근방섹터에 플레이어가 추가 혹은 삭제되는것이 아니기에 9개 섹터에 SHARED로 락을 건다
	// 전부 SC_MOVE_STOP 패킷 쏜 뒤 락풀고 리턴
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(&sectorAround, newSector);
	AcquireSectorAroundShared(&sectorAround);
	for (BYTE i = 0; i < sectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[sectorAround.Around[i].shY][sectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAncient = LinkToClient(pLink);
			// SC_MOVE_STOP은 당사자를 제외하고 수신해야한다
			if (pAncient == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}
			// 원주민의 SessionId만 사용하기에 락을 걸지 않앗다
			Packet* pSC_MOVE_STOP = Packet::Alloc();
			MAKE_SC_MOVE_STOP(pPlayer->dwID, pPlayer->viewDir, playerPos, pSC_MOVE_STOP);
			g_GameServer.SendPacket(pAncient->SessionId, pSC_MOVE_STOP);
			pLink = pLink->pNext;
		}
	}
	ReleaseSectorAroundShared(&sectorAround);
	ReleaseSRWLockExclusive(&pPlayer->playerLock);
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
			st_Client* pDamageViewer = LinkToClient(pLink);
			Packet* pDamagePacket = Packet::Alloc();
			MAKE_SC_DAMAGE(pAttacker->dwID, pVictim->dwID, pVictim->chHp, pDamagePacket);

			// 이미 섹터에 락이 걸려잇으며,x,y,hp,movedir,viewdir,등의 정보가 필요 없기 때문에 DamageView에 락을걸지 않음
			g_GameServer.SendPacket(pDamageViewer->SessionId, pDamagePacket);
			pLink = pLink->pNext;
		}
	}
}

BOOL CS_ATTACK1(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);

	// 피격판정시 데미지 패킷을 보낼 섹터를 구해서 락을 건다
	st_SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// 공격모션을 위해 SC_ATTACK을 주변 9개의 섹터에서 자신을 제외한 플레이어들에게 보내야한다
	for (BYTE i = 0; i < attackSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAttackViewer = LinkToClient(pLink);

			// 공격자 본인은 빼고 보내야한다
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			// 그냥 AttackViewer의 SessionID만 필요해서 락을 걸지 않았음.
			Packet* pSC_ATTACK1 = Packet::Alloc();
			MAKE_SC_ATTACK1(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK1);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK1);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround는 AttackSectorAround의 부분집합이어야하며 락을 거는데 쓰는게아니라 피격판정을 위해 섹터의 플레이어리스트를 순회하는곳에만 사용한다
	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(pPlayer, dfATTACK1_RANGE_Y, dfATTACK1_RANGE_X, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);

	if (!pVictim)
	{
		ReleaseSRWLockShared(&pPlayer->playerLock);
		return FALSE;
	}

	// 이 시점에서 피격자는 EXCLUSIVE로 락이 걸려잇으니 Release될 위험이 없으며 Update에 의해 움직여지지 않는다.

	// SC_DAMAGE 날릴 섹터 구하기
	st_SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE날릴 섹터락 SHARED로 따고 피깎고 데미지패킷 보내고, 섹터락 풀고, 피격자 Exclusive락 풀고, 반환
	AcquireSectorAroundShared(&damageSectorAround);
	pVictim->chHp -= dfATTACK1_DAMAGE;
	DamageProc(pPlayer, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	ReleaseSRWLockShared(&pPlayer->playerLock);
	return TRUE;
}

BOOL CS_ATTACK2(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);

	// 피격판정시 데미지 패킷을 보낼 섹터를 구해서 락을 건다
	st_SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// 공격모션을 위해 SC_ATTACK을 주변 9개의 섹터에서 자신을 제외한 플레이어들에게 보내야한다
	for (BYTE i = 0; i < attackSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAttackViewer = LinkToClient(pLink);

			// 공격자 본인은 빼고 보내야한다
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			// 그냥 AttackViewer의 SessionID만 필요해서 락을 걸지 않았음.
			Packet* pSC_ATTACK2 = Packet::Alloc();
			MAKE_SC_ATTACK2(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK2);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK2);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround는 AttackSectorAround의 부분집합이어야하며 락을 거는데 쓰는게아니라 피격판정을 위해 섹터의 플레이어리스트를 순회하는곳에만 사용한다
	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK2_RANGE_Y, dfATTACK2_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(pPlayer, dfATTACK2_RANGE_Y, dfATTACK2_RANGE_X, &collisionSectorAround);
	ReleaseSectorAroundShared(&attackSectorAround);

	if (!pVictim)
	{
		ReleaseSRWLockShared(&pPlayer->playerLock);
		return FALSE;
	}

	// 이 시점에서 피격자는 EXCLUSIVE로 락이 걸려잇으니 Release될 위험이 없으며 Update에 의해 움직여지지 않는다.
	// SC_DAMAGE 날릴 섹터 구하기
	st_SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE날릴 섹터락 SHARED로 따고 피깎고 데미지패킷 보내고, 섹터락 풀고, 피격자 Exclusive락 풀고, 반환
	AcquireSectorAroundShared(&damageSectorAround);
	pVictim->chHp -= dfATTACK2_DAMAGE;
	DamageProc(pPlayer, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	ReleaseSRWLockShared(&pPlayer->playerLock);
	return TRUE;
}

BOOL CS_ATTACK3(st_Client* pPlayer, MOVE_DIR viewDir, Pos playerPos)
{
	AcquireSRWLockShared(&pPlayer->playerLock);

	// 피격판정시 데미지 패킷을 보낼 섹터를 구해서 락을 건다
	st_SECTOR_AROUND attackSectorAround;
	GetSectorAround(&attackSectorAround, CalcSector(playerPos));
	AcquireSectorAroundShared(&attackSectorAround);

	// 공격모션을 위해 SC_ATTACK을 주변 9개의 섹터에서 자신을 제외한 플레이어들에게 보내야한다
	for (BYTE i = 0; i < attackSectorAround.byCount; ++i)
	{
		st_SECTOR_CLIENT_INFO* pSCI = &g_Sector[attackSectorAround.Around[i].shY][attackSectorAround.Around[i].shX];
		LINKED_NODE* pLink = pSCI->pClientLinkHead;
		for (DWORD j = 0; j < pSCI->dwNumOfClient; ++j)
		{
			st_Client* pAttackViewer = LinkToClient(pLink);

			// 공격자 본인은 빼고 보내야한다
			if (pAttackViewer == pPlayer)
			{
				pLink = pLink->pNext;
				continue;
			}

			// 그냥 AttackViewer의 SessionID만 필요해서 락을 걸지 않았음.
			Packet* pSC_ATTACK3 = Packet::Alloc();
			MAKE_SC_ATTACK3(pPlayer->dwID, pPlayer->viewDir, pPlayer->pos, pSC_ATTACK3);
			g_GameServer.SendPacket(pAttackViewer->SessionId, pSC_ATTACK3);
			pLink = pLink->pNext;
		}
	}

	// collisionSectorAround는 AttackSectorAround의 부분집합이어야하며 락을 거는데 쓰는게아니라 피격판정을 위해 섹터의 플레이어리스트를 순회하는곳에만 사용한다
	st_SECTOR_AROUND collisionSectorAround;
	FindSectorInAttackRange(playerPos, viewDir, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);
	st_Client* pVictim = HandleCollision_AND_ACQUIRE_EXCLUSIVE_ON_HIT(pPlayer, dfATTACK3_RANGE_Y, dfATTACK3_RANGE_X, &collisionSectorAround);

	ReleaseSectorAroundShared(&attackSectorAround);

	if (!pVictim)
	{
		ReleaseSRWLockShared(&pPlayer->playerLock);
		return FALSE;
	}

	// 이 시점에서 피격자는 EXCLUSIVE로 락이 걸려잇으니 Release될 위험이 없으며 Update에 의해 움직여지지 않는다.
	// SC_DAMAGE 날릴 섹터 구하기
	st_SECTOR_AROUND damageSectorAround;
	GetSectorAround(&damageSectorAround, CalcSector(pVictim->pos));

	// SC_DAMAGE날릴 섹터락 SHARED로 따고 피깎고 데미지패킷 보내고, 섹터락 풀고, 피격자 Exclusive락 풀고, 반환
	AcquireSectorAroundShared(&damageSectorAround);
	pVictim->chHp -= dfATTACK2_DAMAGE;
	DamageProc(pPlayer, pVictim, &damageSectorAround);
	ReleaseSectorAroundShared(&damageSectorAround);
	ReleaseSRWLockExclusive(&pVictim->playerLock);
	ReleaseSRWLockShared(&pPlayer->playerLock);
	return TRUE;
}



BOOL CS_ECHO(st_Client* pClient, DWORD dwTime)
{
	Packet* pSC_ECHO = Packet::Alloc();
	MAKE_SC_ECHO(dwTime, pSC_ECHO);
	g_GameServer.SendPacket(pClient->SessionId, pSC_ECHO);
	return TRUE;
}

