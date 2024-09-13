#pragma once
#include "LinkedList.h"
#include "Position.h"
#include "ID.h"
#include "Constant.h"
#include "MOVE_DIR.h"

struct HitInfo
{
	LINKED_NODE hitLink;
	ID BackUpSessionId;
};

struct Player
{
	ID SessionId;
	SRWLOCK playerLock;
	DWORD dwID;
	Pos pos;
	MOVE_DIR viewDir;
	MOVE_DIR moveDir;
	CHAR hp;
	LINKED_NODE SectorLink;
	HitInfo hitInfo;
	DWORD dwUpdateArrIdx;
	BOOL bMoveOrStopInProgress;
#pragma warning(disable : 26495)
	Player()
	{
		InitializeSRWLock(&playerLock);
	}
#pragma warning(default : 26495)

	__forceinline void Init(ID id, Pos pos)
	{
		this->SessionId = id;
		this->dwID = ServerIDToClientID(id);
		this->pos = pos;
		this->viewDir = MOVE_DIR_LL;
		this->moveDir = MOVE_DIR_NOMOVE;
		this->hp = INIT_HP;
		this->bMoveOrStopInProgress = FALSE;
	}
};

#pragma optimize("",on)
__forceinline Player* LinkToClient(LINKED_NODE* pLink)
{
	Player* pRet;
	pRet = (Player*)((char*)pLink - offsetof(Player, SectorLink));
	return pRet;
}
#pragma optimize("",off)
