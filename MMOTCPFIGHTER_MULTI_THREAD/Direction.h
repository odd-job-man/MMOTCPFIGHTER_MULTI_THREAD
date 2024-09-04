#pragma once
#pragma once
constexpr BYTE dfPACKET_MOVE_DIR_LL = 0;
constexpr BYTE dfPACKET_MOVE_DIR_LU = 1;
constexpr BYTE dfPACKET_MOVE_DIR_UU = 2;
constexpr BYTE dfPACKET_MOVE_DIR_RU = 3;
constexpr BYTE dfPACKET_MOVE_DIR_RR = 4;
constexpr BYTE dfPACKET_MOVE_DIR_RD = 5;
constexpr BYTE dfPACKET_MOVE_DIR_DD = 6;
constexpr BYTE dfPACKET_MOVE_DIR_LD = 7;
constexpr BYTE dfPACKET_MOVE_DIR_NOMOVE = 8;

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

// 이동후 OldSector에서 현재 캐릭터의 이동방향을 해당 배열의 인덱스로 대입해서 얻은 방향을 GetDeltaSector에 대입
constexpr BYTE removeDirArr[8] =
{
	dfPACKET_MOVE_DIR_RU, //LL
	dfPACKET_MOVE_DIR_RU, //LU
	dfPACKET_MOVE_DIR_RD, //UU
	dfPACKET_MOVE_DIR_RD, //RU
	dfPACKET_MOVE_DIR_LD, //RR
	dfPACKET_MOVE_DIR_LD, //RD
	dfPACKET_MOVE_DIR_LU, //DD
	dfPACKET_MOVE_DIR_LU, //LD
};

// 이동후 NewSecotr에서 현재 캐릭터의 이동방향을 해당 배열의 인덱스로 대입해서 얻은 방향을 GetDeltaSctor에 대입
constexpr BYTE AddDirArr[8] =
{
	dfPACKET_MOVE_DIR_LD, //LL
	dfPACKET_MOVE_DIR_LD, //LU
	dfPACKET_MOVE_DIR_LU, //UU
	dfPACKET_MOVE_DIR_LU, //RU,
	dfPACKET_MOVE_DIR_RU, //RR
	dfPACKET_MOVE_DIR_RU, //RD
	dfPACKET_MOVE_DIR_RD, //DD
	dfPACKET_MOVE_DIR_RD, //LD
};

constexpr BYTE SectorMoveDir[3][3]
{

	{dfPACKET_MOVE_DIR_LU,dfPACKET_MOVE_DIR_UU,dfPACKET_MOVE_DIR_RU} ,
	{dfPACKET_MOVE_DIR_LL,dfPACKET_MOVE_DIR_NOMOVE,dfPACKET_MOVE_DIR_RR},
	{dfPACKET_MOVE_DIR_LD,dfPACKET_MOVE_DIR_DD,dfPACKET_MOVE_DIR_RD}
};
