#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "OverrunCrowdArrivalTrait.generated.h"

USTRUCT()
struct OVERRUN_API FOverrunCrowdArrivalParameters : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Arrival", meta = (ClampMin = "1", Units = "cm"))
	float StopDistance = 100.f;

	UPROPERTY(EditAnywhere, Category = "Arrival", meta = (ClampMin = "1", Units = "cm"))
	float ResumeDistance = 150.f;

	UPROPERTY(EditAnywhere, Category = "Approach", meta = (ClampMin = "1", Units = "cm"))
	float CloseDistance = 300.f;

	UPROPERTY(EditAnywhere, Category = "Arrival", meta = (ClampMin = "0", Units = "cm"))
	float ArrivalTolerance = 2.f;

	UPROPERTY(EditAnywhere, Category = "Approach", meta = (ClampMin = "0.05", Units = "s"))
	float ReactionTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "0", Units = "cm"))
	float SeparationPadding = 12.f;

	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "0"))
	float SeparationStiffness = 120.f;

	FOverrunCrowdArrivalParameters GetValidated() const
	{
		FOverrunCrowdArrivalParameters Copy = *this;
		Copy.StopDistance = FMath::Max(1.f, Copy.StopDistance);
		Copy.ArrivalTolerance = FMath::Clamp(Copy.ArrivalTolerance, 0.f, 10.f);
		Copy.ResumeDistance = FMath::Max(Copy.StopDistance + Copy.ArrivalTolerance + 1.f, Copy.ResumeDistance);
		Copy.CloseDistance = FMath::Max(Copy.ResumeDistance + 1.f, Copy.CloseDistance);
		Copy.ReactionTime = FMath::Max(0.05f, Copy.ReactionTime);
		Copy.SeparationPadding = FMath::Clamp(Copy.SeparationPadding, 0.f, 100.f);
		Copy.SeparationStiffness = FMath::Clamp(Copy.SeparationStiffness, 0.f, 1000.f);
		return Copy;
	}

	bool ShouldHold(bool bWasHolding, bool bHasTarget, double Distance) const
	{
		return bHasTarget && Distance <= (bWasHolding ? double(ResumeDistance) : double(StopDistance + ArrivalTolerance));
	}
};

USTRUCT()
struct OVERRUN_API FOverrunCrowdArrivalFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector TargetLocation = FVector::ZeroVector;
	FVector HoldLocation = FVector::ZeroVector;
	double DistanceToTarget = TNumericLimits<double>::Max();
	bool bHasTarget = false;
	bool bHolding = false;
};

UCLASS(meta = (DisplayName = "Overrun Crowd Arrival"))
class OVERRUN_API UOverrunCrowdArrivalTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Arrival")
	FOverrunCrowdArrivalParameters Parameters;
};
