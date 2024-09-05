#pragma once
#include "Packet.h"
#include "Position.h"
#include "MOVE_DIR.h"

constexpr int dfPACKET_SC_CREATE_MY_CHARACTER = 0;
constexpr int dfPACKET_SC_CREATE_OTHER_CHARACTER = 1;
constexpr int dfPACKET_SC_DELETE_CHARACTER = 2;
constexpr int dfPACKET_SC_MOVE_START = 11;
constexpr int dfPACKET_SC_MOVE_STOP = 13;
constexpr int dfPACKET_SC_ATTACK1 = 21;
constexpr int dfPACKET_SC_ATTACK2 = 23;
constexpr int dfPACKET_SC_ATTACK3 = 25;
constexpr int dfPACKET_SC_DAMAGE = 30;
constexpr int dfPACKET_SC_SYNC = 251;
constexpr int dfPACKET_SC_ECHO = 253;

void MAKE_SC_CREATE_MY_CHARACTER(DWORD dwDestId, MOVE_DIR viewDir, Pos pos, CHAR chHP, Packet* pPacket);
void MAKE_SC_CREATE_OTHER_CHARACTER(DWORD dwCreateId, MOVE_DIR viewDir, Pos pos, CHAR chHP, Packet* pPacket);
void MAKE_SC_DELETE_CHARACTER(DWORD dwDeleteId, Packet* pPacket);
void MAKE_SC_MOVE_START(DWORD dwStartId, MOVE_DIR moveDir, Pos pos, Packet* pPacket);
void MAKE_SC_SYNC(DWORD dwSyncId, Pos pos, Packet* pPacket);
DWORD MAKE_SC_MOVE_STOP(DWORD dwStopId, BYTE byViewDir, Pos pos, Packet* pPacket);
DWORD MAKE_SC_ATTACK(DWORD dwAttackerId, BYTE byViewDir, Pos AttackerPos, DWORD dwAttackNum, Packet* pPacket);
DWORD MAKE_SC_DAMAGE(DWORD dwAttackerId, DWORD dwVictimId, BYTE byVimctimHp, Packet* pPacket);
void MAKE_SC_SYNC(DWORD dwSyncId, Pos pos, Packet* pPacket);
DWORD MAKE_SC_ECHO(DWORD dwTime, Packet* pPacket);
