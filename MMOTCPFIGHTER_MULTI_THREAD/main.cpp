#include <Winsock2.h>
#include <windows.h>
#include "Stack.h"
#include "GameServer.h"
#include <timeapi.h>
#pragma comment(lib,"Winmm.lib")

GameServer g_GameServer;

int g_iOldFrameTick;
int g_iFpsCheck;
int g_iTime;
int g_iFPS;
int g_iFirst;
int g_iLoop;

constexpr int TICK_PER_FRAME = 40;
constexpr int FRAME_PER_SECONDS = (1000) / TICK_PER_FRAME;

void InitSectionLock();

int main()
{
	InitSectionLock();
	g_GameServer.Start(7000);

	g_iFpsCheck = g_iTime = g_iOldFrameTick = g_iFirst = timeGetTime();
	g_iFPS = 0;

	while (1)
	{
		++g_iLoop;
		g_iTime = timeGetTime();
		if (g_iTime - g_iOldFrameTick >= TICK_PER_FRAME)
		{
			//Update();
			g_iOldFrameTick = g_iTime - ((g_iTime - g_iFirst) % TICK_PER_FRAME);
			++g_iFPS;
		}

		if (g_iTime - g_iFpsCheck >= 1000)
		{
			g_iFpsCheck += 1000;
			g_iLoop = 0;
			g_iFPS = 0;
		}
	}

}