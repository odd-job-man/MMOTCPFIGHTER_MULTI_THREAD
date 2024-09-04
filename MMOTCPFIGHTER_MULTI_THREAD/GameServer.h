#pragma once
#include "IHandler.h"
#include "LanServer.h"

class GameServer : public LanServer
{
	void* OnAccept(ID id);
	void OnRecv(ID id, Packet* pPacket);
};
