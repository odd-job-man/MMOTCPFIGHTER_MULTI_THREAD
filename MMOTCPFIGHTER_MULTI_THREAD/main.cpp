#include <Winsock2.h>
#include <windows.h>
#include "Stack.h"
#include "IHandler.h"
#include "GameServer.h"
#include "Constant.h"
#include <timeapi.h>
#pragma comment(lib,"Winmm.lib")
#include <stdio.h>

GameServer g_GameServer;

int g_iOldFrameTick;
int g_iFpsCheck;
int g_iTime;
int g_iFPS;
int g_iFirst;

constexpr int TICK_PER_FRAME = 20;
constexpr int FRAME_PER_SECONDS = (1000) / TICK_PER_FRAME;

void InitSectionLock();
void Update();

int main()
{
	timeBeginPeriod(1);
	InitSectionLock();
	g_GameServer.Start(MAX_SESSION);

	g_iFpsCheck = g_iTime = g_iOldFrameTick = g_iFirst = timeGetTime();
	g_iFPS = 0;
	while (1)
	{
		Update();
		g_iTime = timeGetTime();
		++g_iFPS;
		if (g_iTime - g_iFpsCheck >= 1000)
		{
			g_iFpsCheck += 1000;
			g_iFPS = 0;
		}

		// 밀린경우
		if (g_iTime - g_iOldFrameTick >= TICK_PER_FRAME)
		{
			g_iOldFrameTick = g_iTime - ((g_iTime - g_iFirst) % TICK_PER_FRAME);
			continue;
		}

		// 안밀린경우
		Sleep(TICK_PER_FRAME - (g_iTime - g_iOldFrameTick));
		g_iOldFrameTick += TICK_PER_FRAME;
	}

}