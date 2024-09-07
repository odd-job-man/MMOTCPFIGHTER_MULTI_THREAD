#pragma once
#include "IHandler.h"
#include "LanServer.h"

class GameServer : public LanServer
{
	void* OnAccept(ID id);
	void OnRecv(void* pClient, Packet* pPacket);
	void OnRelease(void* pClient);
	BOOL IsNetworkStateInvalid(ID SessionId);
	int GetAllValidClient(void** ppOutClientArr);
public:
	void Update();
};

//BOOL GetAllValidClient(DWORD* pOutUserNum, void** ppOutClientArr)
//{
//	DWORD dwClientNum = 0;
//	for (DWORD i = 0; i < g_dwSessionNum; ++i)
//	{
//		if (g_pSessionArr[i]->IsValid == INVALID)
//			continue;
//
//		ppOutClientArr[dwClientNum] = g_pSessionArr[i]->pClient;
//		++dwClientNum;
//	}
//
//	*pOutUserNum = dwClientNum;
//	return TRUE;
//}
