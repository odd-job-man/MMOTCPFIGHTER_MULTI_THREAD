#pragma once
enum MOVE_DIR : BYTE
{
	MOVE_DIR_LL = 0,
	MOVE_DIR_LU,
	MOVE_DIR_UU,
	MOVE_DIR_RU,
	MOVE_DIR_RR,
	MOVE_DIR_RD,
	MOVE_DIR_DD,
	MOVE_DIR_LD,
	MOVE_DIR_NOMOVE
};

#define MOVE_DIR_TO_BYTE(dir) (*((BYTE*)dir))

// LL UU RR DD가 0 2 4 6이라서 그럼, NOMOVE넣을 사람은 없을것이다.
__forceinline BOOL IsMovingStraight(MOVE_DIR SectorMoveDir)
{
	return (SectorMoveDir % 2 == 0);
}

__forceinline BOOL IsMovingVertical(MOVE_DIR SectorMoveDir)
{
	return (SectorMoveDir == MOVE_DIR_UU || SectorMoveDir == MOVE_DIR_DD);
}

__forceinline BOOL IsMovingHorizon(MOVE_DIR SectorMoveDir)
{
	return (SectorMoveDir == MOVE_DIR_RR || SectorMoveDir == MOVE_DIR_LL);
}

