#pragma once
#include <windows.h>
#include "LinkedList.h"
#include "Position.h"
#include "ID.h"
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
};

#pragma optimize("",on)
__forceinline st_Client* LinkToClient(LINKED_NODE* pLink)
{
	st_Client* pRet;
	pRet = (st_Client*)((char*)pLink - offsetof(st_Client, SectorLink));
	return pRet;
}
#pragma optimize("",off)
