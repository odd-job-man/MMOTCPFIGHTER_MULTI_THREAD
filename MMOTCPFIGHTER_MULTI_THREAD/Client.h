#pragma once
#include "LinkedList.h"
#include "Position.h"
#include "ID.h"
#include "Constant.h"
#include "MOVE_DIR.h"
struct st_Client
{
	ID SessionId;
	SRWLOCK clientLock;
	DWORD dwID;
	Pos pos;
	MOVE_DIR viewDir;
	MOVE_DIR moveDir;
	CHAR chHp;
	SectorPos CurSector;
	SectorPos OldSector;
	LINKED_NODE SectorLink;

#pragma warning(disable : 26495)
	st_Client()
	{
		InitializeSRWLock(&clientLock);
	}
#pragma warning(default : 26495)

	__forceinline void Init(ID id, Pos pos, SectorPos sectorPos)
	{
		this->SessionId = id;
		this->dwID = ServerIDToClientID(id);
		this->pos = pos;
		this->viewDir = MOVE_DIR_LL;
		this->moveDir = MOVE_DIR_NOMOVE;
		this->chHp = INIT_HP;
		this->CurSector = sectorPos;
		this->OldSector = sectorPos;
	}
};

#pragma optimize("",on)
__forceinline st_Client* LinkToClient(LINKED_NODE* pLink)
{
	st_Client* pRet;
	pRet = (st_Client*)((char*)pLink - offsetof(st_Client, SectorLink));
	return pRet;
}
#pragma optimize("",off)
