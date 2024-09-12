#pragma once
#include <WinSock2.h>
#include <windows.h>
#include "Packet.h"

#include "Session.h"
class IHandler
{
public:
	virtual BOOL Start(DWORD dwMaxSession) = 0;
	virtual void SendPacket(ID id, Packet* pPacket) = 0;
	virtual BOOL OnConnectionRequest() = 0;
	virtual void* OnAccept(ID id) = 0;
	virtual void OnRecv(void* pClient, Packet* pPacket) = 0;
	virtual void OnRelease(void* pPlayer) = 0;
private:
	virtual BOOL SendPost(Session* pSession) = 0;
	virtual BOOL RecvPost(Session* pSession) = 0;
	virtual void ReleaseSession(Session* pSession) = 0;
};
