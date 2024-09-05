#pragma once
#include "MOVE_DIR.h"


constexpr BYTE MOVE = 1;
constexpr BYTE NOMOVE = 0;

union DirVector
{
	struct
	{
		SHORT shY;
		SHORT shX;
	};
	int YX;
};

constexpr DirVector spArrDir[9]
{
	DirVector{-1,-1},
	DirVector{-1,0},//LU
	DirVector{-1,1},//UU
	DirVector{0,-1}, //RU
	DirVector{0,0}, //RR
	DirVector{0,1}, //RD
	DirVector{1,-1}, // DD
	DirVector{1,0}, //LD
	DirVector{1,1} //LD
};

constexpr DirVector vArr[8]{
	DirVector{0,-1},
	DirVector{-1,-1},//LU
	DirVector{-1,0},//UU
	DirVector{-1,1}, //RU
	DirVector{0,1}, //RR
	DirVector{1,1}, //RD
	DirVector{1,0}, // DD
	DirVector{1,-1} //LD
};

// �̵��� OldSector���� ���� ĳ������ �̵������� �ش� �迭�� �ε����� �����ؼ� ���� ������ GetDeltaSector�� ����
constexpr MOVE_DIR removeDirArr[8] =
{
	MOVE_DIR_RU, //LL
	MOVE_DIR_RU, //LU
	MOVE_DIR_RD, //UU
	MOVE_DIR_RD, //RU
	MOVE_DIR_LD, //RR
	MOVE_DIR_LD, //RD
	MOVE_DIR_LU, //DD
	MOVE_DIR_LU, //LD
};

// �̵��� NewSecotr���� ���� ĳ������ �̵������� �ش� �迭�� �ε����� �����ؼ� ���� ������ GetDeltaSctor�� ����
constexpr MOVE_DIR AddDirArr[8] =
{
	MOVE_DIR_LD, //LL
	MOVE_DIR_LD, //LU
	MOVE_DIR_LU, //UU
	MOVE_DIR_LU, //RU,
	MOVE_DIR_RU, //RR
	MOVE_DIR_RU, //RD
	MOVE_DIR_RD, //DD
	MOVE_DIR_RD, //LD
};

constexpr MOVE_DIR SectorMoveDir[3][3]
{

	{MOVE_DIR_LU,MOVE_DIR_UU,MOVE_DIR_RU} ,
	{MOVE_DIR_LL,MOVE_DIR_NOMOVE,MOVE_DIR_RR},
	{MOVE_DIR_LD,MOVE_DIR_DD,MOVE_DIR_RD}
};
