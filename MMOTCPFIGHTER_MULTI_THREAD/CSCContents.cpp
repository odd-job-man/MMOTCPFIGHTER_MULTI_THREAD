#include <windows.h>
#include "Direction.h"
#include "CSCContents.h"
#include "SCCContents.h"
#include "Sector.h"
#include "Stack.h"
#include "GameServer.h"
extern GameServer g_GameServer;

BOOL CS_MOVE_START(st_Client* pClient, MOVE_DIR moveDir, Pos clientPos)
{
	// 클라이언트 시선처리
	AcquireSRWLockExclusive(&pClient->clientLock);
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
	ReleaseSRWLockExclusive(&pClient->clientLock);

	AcquireSRWLockShared(&pClient->clientLock);
	SectorPos oldSector = CalcSector(pClient->pos);

	// 이동중에 방향을 바꿔서 STOP이 오지않고 또 다시 START가 왓는데, 이때 서버 프레임이 떨어져서 싱크가 발생하는 경우.
	// 기존 서버 좌표 기준으로 근방 섹터에 SYNC메시지를 날려야하며 따라서 그 서버좌표 근방섹터에 전부 락을잡는다
	if (IsSync(pClient->pos, clientPos))
	{
		LockAroundSectorShared(oldSector);
		AroundInfo* pAroundInfo = GetAroundValidClient(oldSector, nullptr);
		for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
		{
			Packet* pSyncPacket = Packet::Alloc();
			g_GameServer.SendPacket(pClient->SessionId, pSyncPacket);
		}
		clientPos = pClient->pos;
		UnLockAroundSectorShared(oldSector);
	}

	SectorPos newSector = CalcSector(pClient->pos);

	// dfERROR_RANGE보다 오차가 작지만, 섹터가 틀려서 섹터를 클라기준으로 맞출경우 업데이트 
	if (!IsSameSector(oldSector, newSector))
	{
		MOVE_DIR bySectorMoveDir = GetSectorMoveDir(oldSector, newSector);
		// 어차피 곧 움직인다고 보낼거기에 지금은 움직인다고 알릴필요 없다.
		SectorUpdateAndNotify(pClient, bySectorMoveDir, oldSector, newSector, FALSE);
	}

	// 같은 섹터라면 현재 위치한 섹터에 내가 움직이기 시작한다고 패킷을 보내야한다.
	// 근방섹터에 플레이어가 추가 혹은 삭제되는것이 아니기에 9개 섹터에 SHARED로 락을 건다
	LockAroundSectorShared(newSector);
	AroundInfo* pAroundInfo = GetAroundValidClient(newSector, pClient);

	// 만약 아무도없으면 그냥 클라상태만 반영하고 락풀고 끝
	if (pAroundInfo->CI.dwNum <= 0)
	{
		pClient->moveDir = moveDir;
		pClient->pos = clientPos;
		UnLockAroundSectorShared(newSector);
		return TRUE;
	}

	// 누군가 있다면 긁어온 유효한 플레이어 목록에 전부 MOVE_START 패킷을 쏜 뒤 락풀고 리턴
	Packet* pMoveStartPacket = Packet::Alloc();
	for (DWORD i = 0; i < pAroundInfo->CI.dwNum; ++i)
	{
		MAKE_SC_MOVE_START(dwFromId, moveDir, clientPos, pMoveStartPacket);
		g_GameServer.SendPacket(pClient->SessionId, pMoveStartPacket);
	}
	UnLockAroundSectorShared(newSector);
	ReleaseSRWLockShared(&pClient->clientLock);
    return TRUE;
}
