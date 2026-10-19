#pragma once

#include "CoreMinimal.h"
#include "MassEntityElementTypes.h"
#include "OverrunEnemyHitPresentationFragment.generated.h"

class AActor;
class UOverrunHitFlashComponent;

USTRUCT()
struct FOverrunEnemyHitPresentationFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> VisualActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UOverrunHitFlashComponent> FlashComponent;

	UPROPERTY(Transient)
	uint32 LastHitSerial = 0;
};