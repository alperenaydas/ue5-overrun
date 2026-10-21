#pragma once

#include "CoreMinimal.h"
#include "MassEntityElementTypes.h"
#include "OverrunEnemyPresentationFragment.generated.h"

class AActor;
class USkeletalMeshComponent;

USTRUCT()
struct FOverrunEnemyPresentationFragment : public FMassFragment
{
	GENERATED_BODY()

	FTransform DisplayTransform = FTransform::Identity;
	FVector PreviousTargetLocation = FVector::ZeroVector;
	float GroundSpeed = 0.f;
	uint64 LastUpdateFrame = 0;
	bool bInitialized = false;
	bool bResetThisFrame = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> VisualActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMesh;

	void Reset() { *this = FOverrunEnemyPresentationFragment(); }
};
