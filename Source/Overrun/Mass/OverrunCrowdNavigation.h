#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"
#include "MassEntityHandle.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "OverrunCrowdNavigation.generated.h"

class APawn;
class ARecastNavMesh;
class UNavigationQueryFilter;

USTRUCT()
struct OVERRUN_API FOverrunCrowdNavigationParameters : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Agent", meta = (ClampMin = "1", Units = "cm"))
	float AgentHeight = 180.f;

	UPROPERTY(EditAnywhere, Category = "Agent", meta = (Units = "cm"))
	float EntityHeightOffset = 0.f;

	UPROPERTY(EditAnywhere, Category = "Agent", meta = (ClampMin = "1", ClampMax = "100", Units = "cm"))
	float ProjectionHeightTolerance = 50.f;

	UPROPERTY(EditAnywhere, Category = "Agent")
	TSubclassOf<UNavigationQueryFilter> QueryFilter;

	UPROPERTY(EditAnywhere, Category = "Routes", meta = (ClampMin = "25", Units = "cm"))
	float GoalMovementThreshold = 100.f;

	UPROPERTY(EditAnywhere, Category = "Routes", meta = (ClampMin = "0.25", Units = "s"))
	float RefreshInterval = 2.f;

	UPROPERTY(EditAnywhere, Category = "Routes", meta = (ClampMin = "0.25", Units = "s"))
	float RetryInterval = 1.f;

	UPROPERTY(EditAnywhere, Category = "Routes", meta = (ClampMin = "1", Units = "s"))
	float NoProgressTimeout = 3.f;

	UPROPERTY(EditAnywhere, Category = "Routes", meta = (ClampMin = "100", Units = "cm"))
	float CorridorWidth = 400.f;
};

USTRUCT()
struct OVERRUN_API FOverrunCrowdNavigationFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassEntityHandle Identity;
	TWeakObjectPtr<APawn> Target;
	TWeakObjectPtr<ARecastNavMesh> NavData;
	FVector Goal = FVector::ZeroVector;
	FVector RequestedGoal = FVector::ZeroVector;
	FVector ExpectedPosition = FVector::ZeroVector;
	FVector ProgressPosition = FVector::ZeroVector;
	double NavPlaneZ = 0.0;
	double NextRequestTime = 0.0;
	double NextRefreshTime = 0.0;
	double QueuedSince = 0.0;
	double ProgressTime = 0.0;
	double NextRecoveryTime = 0.0;
	NavNodeRef DirectStartNode = INVALID_NAVNODEREF;
	NavNodeRef WallSourceNode = INVALID_NAVNODEREF;
	bool bInitialized = false;
	bool bQueued = false;
	bool bRecoveryRepath = false;
	bool bHasRoute = false;
	bool bDirectPath = false;
	bool bPartial = false;
	bool bFinalSegment = false;
	bool bCanMove = false;
};

UCLASS(meta = (DisplayName = "Overrun Crowd Navigation"))
class OVERRUN_API UOverrunCrowdNavigationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Navigation")
	FOverrunCrowdNavigationParameters Parameters;
};
