#pragma once
#include "../Framework.h"

RS_NS_START

enum class GameMode : byte {
	SOCCAR,
	HOOPS,
	HEATSEEKER,
	SNOWDAY,
	DROPSHOT,

	// I will not add rumble unless I am given a large amount of money, or, alternatively, a large amount of candy corn (I love candy corn)

	// Soccar but without goals, boost pads, or the arena hull. The cars and ball will fall infinitely.
	THE_VOID,
	
	// Like THE_VOID but with a ground plane for testing purposes
	THE_VOID_WITH_GROUND,
};

constexpr const char* GAMEMODE_STRS[] = {
	"soccar",
	"hoops",
	"heatseeker",
	"snowday",
	"dropshot",
	"void",
	"void_with_ground"
};

RS_NS_END