#pragma once
#include "MassEntityHandle.h"

struct FOverrunEnemyDamageCommand
{
	FMassEntityHandle TargetEntity = FMassEntityHandle();
	float Damage = 0.f;
};
