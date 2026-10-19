#pragma once
#include "MassEntityElementTypes.h"
#include "OverrunEnemyHitShapeFragment.generated.h"

USTRUCT()
struct FOverrunEnemyHitShapeFragment : public FMassFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "")
	float Radius = 40.f;
	UPROPERTY(EditAnywhere, Category = "")
	float HalfHeight = 90.f;
	UPROPERTY(EditAnywhere, Category = "")
	float CenterZOffset = 0.f;
};
