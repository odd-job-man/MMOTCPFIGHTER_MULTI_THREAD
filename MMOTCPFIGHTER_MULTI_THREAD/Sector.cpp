#include "Sector.h"

void LockAroundSector(SectorPos sector)
{
	SectorPos sectorToLock;
	for (DWORD i = 0; i < 9; ++i)
	{
		sectorToLock.shX = sector.shX + spArrDir[i].shX;
		sectorToLock.shY = sector.shY + spArrDir[i].shY;
		if (!IsValidSector(sectorToLock))
			continue;
		AcquireSRWLockShared(&g_Sector[sectorToLock.shY][sectorToLock.shX].srwSectionLock);
	}
}
void UnLockAroundSector(SectorPos sector)
{
	SectorPos sectorToUnlock;
	for (DWORD i = 0; i < 9; ++i)
	{
		sectorToUnlock.shX = sector.shX + spArrDir[i].shX;
		sectorToUnlock.shY = sector.shY + spArrDir[i].shY;
		if (!IsValidSector(sectorToUnlock))
			continue;
		ReleaseSRWLockShared(&g_Sector[sectorToUnlock.shY][sectorToUnlock.shX].srwSectionLock);
	}
}

void AddClientAtSector(st_Client* pClient, SectorPos newSectorPos)
{
	LinkToLinkedListLast(&(g_Sector[newSectorPos.shY][newSectorPos.shX].pClientLinkHead), &(g_Sector[newSectorPos.shY][newSectorPos.shX].pClientLinkTail), &(pClient->SectorLink));
	++(g_Sector[newSectorPos.shY][newSectorPos.shX].dwNumOfClient);
}

void RemoveClientAtSector(st_Client* pClient, SectorPos oldSectorPos)
{
	UnLinkFromLinkedList(&(g_Sector[oldSectorPos.shY][oldSectorPos.shX].pClientLinkHead), &(g_Sector[oldSectorPos.shY][oldSectorPos.shX].pClientLinkTail), &(pClient->SectorLink));
	--(g_Sector[oldSectorPos.shY][oldSectorPos.shX].dwNumOfClient);
}

