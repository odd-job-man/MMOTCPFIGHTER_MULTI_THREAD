#pragma once
#pragma once
#define SERVERPORT  11402
#define dfPACKET_CODE		0x89

//-----------------------------------------------------------------
// 30�� �̻��� �ǵ��� �ƹ��� �޽��� ���ŵ� ���°�� ���� ����.
//-----------------------------------------------------------------
constexpr int dfNETWORK_PACKET_RECV_TIMEOUT = 30000;

//-----------------------------------------------------------------
// ȭ�� �̵� ����.
//-----------------------------------------------------------------
constexpr int dfRANGE_MOVE_TOP = 0;
constexpr int dfRANGE_MOVE_LEFT = 0;
constexpr int dfRANGE_MOVE_RIGHT = 6400;
constexpr int dfRANGE_MOVE_BOTTOM = 6400;

constexpr int dfERROR_RANGE = 50;
constexpr int MOVE_UNIT_X = 3;
constexpr int MOVE_UNIT_Y = 2;

constexpr int INIT_POS_X = 300;
constexpr int INIT_POS_Y = 300;
constexpr int INIT_HP = 100;

//---------------------------------------------------------------
// ���ݹ���.
//---------------------------------------------------------------
constexpr int dfATTACK1_RANGE_X = 80;
constexpr int dfATTACK2_RANGE_X = 90;
constexpr int dfATTACK3_RANGE_X = 100;
constexpr int dfATTACK1_RANGE_Y = 10;
constexpr int dfATTACK2_RANGE_Y = 10;
constexpr int dfATTACK3_RANGE_Y = 20;

//---------------------------------------------------------------
// ���� ������.
//---------------------------------------------------------------
constexpr int dfATTACK1_DAMAGE = 1;
constexpr int dfATTACK2_DAMAGE = 2;
constexpr int dfATTACK3_DAMAGE = 3;

//-----------------------------------------------------------------
// ĳ���� �̵� �ӵ�   // 50fps ���� �̵��ӵ�
//-----------------------------------------------------------------
constexpr int dfSPEED_PLAYER_X = 6;
constexpr int dfSPEED_PLAYER_Y = 4;

constexpr int df_SECTOR_WIDTH = 160;
constexpr int df_SECTOR_HEIGHT = 160;
constexpr int dwNumOfSectorHorizon = dfRANGE_MOVE_RIGHT / df_SECTOR_WIDTH;
constexpr int dwNumOfSectorVertical = dfRANGE_MOVE_BOTTOM / df_SECTOR_HEIGHT;

constexpr DWORD dwTimeOut = 30 * 1000;
constexpr DWORD MAX_SESSION = 10000;
