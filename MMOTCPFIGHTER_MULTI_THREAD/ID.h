#pragma once

union ID
{
	ULONGLONG ullId;
};

__forceinline ID MAKE_SESSION_ID(ULONGLONG ullCounter, SHORT shIdx)
{
	ID id;
	id.ullId = ullCounter << 16;
	id.ullId ^= shIdx;
	return id;
}

__forceinline SHORT GET_SESSION_INDEX(ID id)
{
	return id.ullId & 0xFFFF;
}

// 기존 프로토콜에서의 ID가 4바이트라 어쩔수없이 억지스럽게 만들어냄
__forceinline DWORD ServerIDToClientID(ID id)
{
	DWORD dwRetId = (DWORD)(id.ullId >> 16);
	return dwRetId;
}

