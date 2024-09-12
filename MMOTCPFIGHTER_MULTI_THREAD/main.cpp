#include <Winsock2.h>
#include <windows.h>
#include "Stack.h"
#include "IHandler.h"
#include "GameServer.h"
#include "Constant.h"
#include <timeapi.h>
#pragma comment(lib,"Winmm.lib")
#include <stdio.h>
#include "client.h"
#include "Update.h"

GameServer g_GameServer;

int g_iOldFrameTick;
int g_iFpsCheck;
int g_iTime;
int g_iFPS;
int g_iFirst;

constexpr int TICK_PER_FRAME = 40;
constexpr int FRAME_PER_SECONDS = (1000) / TICK_PER_FRAME;

void InitSectionLock();

int main()
{
	timeBeginPeriod(1);
	InitSectionLock();
	InitPlayerArrLock();

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
			FILETIME ftCreationTime, ftExitTime, ftKernelTime, ftUsertTime;
			FILETIME ftCurTime;
			GetProcessTimes(GetCurrentProcess(), &ftCreationTime, &ftExitTime, &ftKernelTime, &ftUsertTime);
			GetSystemTimeAsFileTime(&ftCurTime);

			ULARGE_INTEGER start, now;
			start.LowPart = ftCreationTime.dwLowDateTime;
			start.HighPart = ftCreationTime.dwHighDateTime;
			now.LowPart = ftCurTime.dwLowDateTime;
			now.HighPart = ftCurTime.dwHighDateTime;

			ULONGLONG ullElapsedSecond = (now.QuadPart - start.QuadPart) / 10000 / 1000;
			ULONGLONG ullElapsedMin = ullElapsedSecond / 60;
			ullElapsedSecond %= 60;

			ULONGLONG ullElapsedHour = ullElapsedMin / 60;
			ullElapsedMin %= 60;

			ULONGLONG ullElapsedDay = ullElapsedHour / 24;
			ullElapsedHour %= 24;

			printf("-----------------------------------------------------\n");
			printf("Elapsed Time : %02lluD-%02lluH-%02lluMin-%02lluSec\n", ullElapsedDay, ullElapsedHour, ullElapsedMin, ullElapsedSecond);
			printf("fps : %d\n", g_iFPS);
			printf("-----------------------------------------------------\n");
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