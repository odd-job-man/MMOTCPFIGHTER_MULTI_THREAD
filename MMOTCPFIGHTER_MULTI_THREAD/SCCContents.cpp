#include "SCCContents.h"
#include "ID.h"

// 기존 프로토콜에서의 ID가 4바이트라 어쩔수없이 억지스럽게 만들어냄
__forceinline DWORD ServerIDToClientID(ID id)
{
	DWORD dwRetId = (DWORD)(id.ullId >> 16);
	return dwRetId;
}

__forceinline void MAKE_HEADER(BYTE byPacketType, Packet* pPacket)
{
	(*pPacket) << byPacketType;
}

void MAKE_SC_CREATE_MY_CHARACTER(ID destId, BYTE byDirection, Pos pos, CHAR chHP, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_CREATE_MY_CHARACTER, pPacket);
	(*pPacket) << ServerIDToClientID(destId) << byDirection << pos.shX << pos.shY << chHP;
}

void MAKE_SC_CREATE_OTHER_CHARACTER(ID CreateId, BYTE byDirection, Pos pos, CHAR chHP, Packet* pPacket)
{
	MAKE_HEADER(dfPACKET_SC_CREATE_OTHER_CHARACTER, pPacket);
	(*pPacket) << ServerIDToClientID(CreateId) << byDirection << pos.shX << pos.shY << chHP;
}
