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

// ���� �������ݿ����� ID�� 4����Ʈ�� ��¿������ ���������� ����
__forceinline DWORD ServerIDToClientID(ID id)
{
	DWORD dwRetId = (DWORD)(id.ullId >> 16);
	return dwRetId;
}

