#pragma once

#include "skse64/NiNodes.h"
#include "skse64/GameData.h"


float VectorLength(NiPoint3 vec);

NiPoint3 VectorNormalized(NiPoint3 vec);

float DotProduct(NiPoint3 vec1, NiPoint3 vec2);

NiAVObject * GetHighestParent(NiAVObject *node);

UInt32 GetFullFormID(const ModInfo * modInfo, UInt32 formLower);
