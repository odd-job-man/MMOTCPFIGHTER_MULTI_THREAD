#pragma once
#include <windows.h>
union SectorPos
{
	int YX;
	struct
	{
		SHORT shY;
		SHORT shX;
	};
};

union Pos
{
	int YX;
	struct
	{
		SHORT shY;
		SHORT shX;
	};
};
