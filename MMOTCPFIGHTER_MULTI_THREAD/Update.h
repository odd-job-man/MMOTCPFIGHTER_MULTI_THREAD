#pragma once
extern SRWLOCK g_srwPlayerArrLock;
extern Player* g_playerArr[MAX_SESSION];
extern DWORD g_dwPlayerNum;

__forceinline void InitPlayerArrLock()
{
	InitializeSRWLock(&g_srwPlayerArrLock);
}

__forceinline void RegisterPlayer(Player* pPlayer)
{
	g_playerArr[g_dwPlayerNum] = pPlayer;
	pPlayer->dwUpdateArrIdx = g_dwPlayerNum;
	++g_dwPlayerNum;
}

__forceinline void ReleasePlayer(Player* pPlayer)
{
	DWORD dwRlsIdx = pPlayer->dwUpdateArrIdx;
	g_playerArr[dwRlsIdx] = g_playerArr[g_dwPlayerNum - 1];
	g_playerArr[dwRlsIdx]->dwUpdateArrIdx = dwRlsIdx;
	--g_dwPlayerNum;
}

void Update();
