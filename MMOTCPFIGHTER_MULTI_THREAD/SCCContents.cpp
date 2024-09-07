#include <windows.h>
#include "ID.h"
#include "MOVE_DIR.h"
#include "SCCContents.h"

__forceinline void MAKE_HEADER(BYTE byPacketType, Packet* pPacket)
{
	(*pPacket) << byPacketType;
}

void MAKE_SC_CREATE_MY_CHARACTER(DWORD dwDestId, MOVE_DIR viewDir, Pos pos, CHAR chHP, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_CREATE_MY_CHARACTER, pPacket);
	(*pPacket) << dwDestId << MOVE_DIR_TO_BYTE(&viewDir) << pos.shX << pos.shY << chHP;
}

void MAKE_SC_CREATE_OTHER_CHARACTER(DWORD dwCreateId, MOVE_DIR viewDir, Pos pos, CHAR chHP, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_CREATE_OTHER_CHARACTER, pPacket);
	(*pPacket) << dwCreateId << MOVE_DIR_TO_BYTE(&viewDir) << pos.shX << pos.shY << chHP;
}

void MAKE_SC_DELETE_CHARACTER(DWORD dwDeleteId, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_DELETE_CHARACTER, pPacket);
	(*pPacket) << dwDeleteId;
}

void MAKE_SC_MOVE_START(DWORD dwStartId, MOVE_DIR moveDir, Pos pos, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_MOVE_START, pPacket);
	(*pPacket) << dwStartId << MOVE_DIR_TO_BYTE(&moveDir) << pos.shX << pos.shY;
}

void MAKE_SC_MOVE_STOP(DWORD dwStopId, MOVE_DIR viewDir, Pos pos, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_MOVE_STOP, pPacket);
	(*pPacket) << dwStopId << MOVE_DIR_TO_BYTE(&viewDir) << pos.shX << pos.shY;
}

void MAKE_SC_ATTACK1(DWORD dwAttackerId, MOVE_DIR viewDir, Pos AttackerPos, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_ATTACK1, pPacket);
	(*pPacket) << dwAttackerId << MOVE_DIR_TO_BYTE(&viewDir) << AttackerPos.shX << AttackerPos.shY;
}

void MAKE_SC_ATTACK2(DWORD dwAttackerId, MOVE_DIR viewDir, Pos AttackerPos, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_ATTACK2, pPacket);
	(*pPacket) << dwAttackerId << MOVE_DIR_TO_BYTE(&viewDir) << AttackerPos.shX << AttackerPos.shY;
}

void MAKE_SC_ATTACK3(DWORD dwAttackerId, MOVE_DIR viewDir, Pos AttackerPos, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_ATTACK3, pPacket);
	(*pPacket) << dwAttackerId << MOVE_DIR_TO_BYTE(&viewDir) << AttackerPos.shX << AttackerPos.shY;
}

void MAKE_SC_DAMAGE(DWORD dwAttackerId, DWORD dwVictimId, CHAR chVimctimHp, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_DAMAGE, pPacket);
	(*pPacket) << dwAttackerId << dwVictimId << chVimctimHp;
}

void MAKE_SC_SYNC(DWORD dwSyncId, Pos pos, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_SYNC, pPacket);
	(*pPacket) << dwSyncId << pos.shX << pos.shY;
}

void MAKE_SC_ECHO(DWORD dwTime, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_ECHO, pPacket);
	(*pPacket) << dwTime;
}
