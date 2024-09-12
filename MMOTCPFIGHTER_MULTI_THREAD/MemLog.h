#pragma once
#define SIZE 1001000
enum EVENT
{
	SECTOR_UPDATE_AND_NOTIFY,
	ACQ_SECTOR_AROUND_SHARED_IF_PLAYER_EXCLUSIVE,
	ACQ_SECTOR_AROUND_EXCLUSIVE_IF_PLAYER_EXCLUSIVE,
	UPDATE,
	ONACCEPT,
	ONRELEASE,
	MOVE_START,
	MOVE_STOP,
	ATTACK1,
	ATTACK2,
	ATTACK3,
	TRY_ACQUIRE_MOVE_LOCK,
	RELASE_MOVE_LOCKS
};

enum ACQ_RLS
{
	ACQUIRE,
	RELEASE,
};

enum MUTUAL
{
	EXCLUSIVE, 
	SHARED,
};

struct MemLog
{
	UINT64 logNum;
	EVENT funcName;
	ACQ_RLS AcquireRelease;
	MUTUAL mutual;
	UINT32 threadId;
};

extern int g_iCounter;
extern MemLog g_logArr[1001000];

__forceinline UINT64 WRITE_MEMORY_LOG(EVENT funcName, MUTUAL mutual, ACQ_RLS AcquireRelease)
{
	UINT64 iCnt = InterlockedIncrement((LONG*)&g_iCounter) - 1;
	int iIndex = iCnt % 1000000;
	g_logArr[iIndex].logNum = iCnt;
	g_logArr[iIndex].funcName = funcName;
	g_logArr[iIndex].AcquireRelease = AcquireRelease;
	g_logArr[iIndex].mutual = mutual;
	g_logArr[iIndex].threadId = GetCurrentThreadId();
	return iCnt;
}
