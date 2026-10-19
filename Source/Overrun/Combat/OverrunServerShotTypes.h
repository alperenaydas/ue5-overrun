#pragma once
#include "CoreMinimal.h"

struct FFireAbilityData
{
	FVector SampledOrigin;
	float CMCMovementTimestamp;
	FVector SampledDestination;
	double ServerReceiptTime;
	
	float Damage = 0.f;
	float MaxOriginError = 0.f;
	
	static constexpr double MaxLifetimeSeconds = 0.25;
};