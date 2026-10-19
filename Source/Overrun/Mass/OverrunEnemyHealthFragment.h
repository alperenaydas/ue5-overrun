#pragma once
#include "MassEntityElementTypes.h"
#include "OverrunEnemyHealthFragment.generated.h"

USTRUCT()
struct FOverrunEnemyHealthFragment : public FMassFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "")
	float Health = 100.f;
	UPROPERTY(EditAnywhere, Category = "")
	float MaxHealth = 100.f;
	UPROPERTY(Transient)
	uint32 HitSerial = 0;
};
