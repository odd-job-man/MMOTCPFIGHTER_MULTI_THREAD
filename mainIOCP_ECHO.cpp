#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>
#include "Stack.h"
#include "IHandler.h"
#include "LanServer.h"
#include "Logger.h"
#define MAX_SESSION 3000

int main()
{
	LanServer ls;
	BOOL bShutDown = FALSE;
	BOOL bStopMode = FALSE;
	LOG(L"SYSTEM", SYSTEM, CONSOLE, L"Server StartUp()!");
	ls.Start(MAX_SESSION);
	while (!bShutDown)
	{
		Sleep(1000);
		// Stop���°� �ƴϸ鼭 ESC�� �������� STOP���� �ٲٰ� ������ �����
		if (!bStopMode && GetAsyncKeyState(VK_SPACE) & 0x01)
		{
			bStopMode = TRUE;
			ls.Stop();
		}

		// stop �����̸鼭 SPACE�� �������� START���·� ������Ѵ�.
		if (bStopMode && GetAsyncKeyState(VK_SPACE) & 0x01)
		{
			bStopMode = FALSE;
			LOG(L"SYSTEM", SYSTEM, CONSOLE, L"Server StartUp()!");
			ls.Start(MAX_SESSION);
		}

		if(bStopMode)
			printf("Is Stop Press SPACE To Restart!\n");
		else
			printf("Is Running Press SPACE To Stop!\n");


		ls.Monitoring();

		if (GetAsyncKeyState(VK_BACK) & 0x01)
		{
			if(!bStopMode)
				ls.Stop();
			bShutDown = TRUE;
		}
	}
	return 0;

}