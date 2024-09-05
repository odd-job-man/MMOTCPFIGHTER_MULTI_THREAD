#include <Winsock2.h>
#include <windows.h>
#include "Stack.h"
#include "GameServer.h"

GameServer g_GameServer;
void InitSectionLock();

int main()
{
	InitSectionLock();
	g_GameServer.Start(7000);
	while (1)
	{
		Sleep(1000);
	}

}