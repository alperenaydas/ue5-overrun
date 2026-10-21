#pragma once

#include "CoreMinimal.h"
#include "MassMovableVisualizationTrait.h"
#include "OverrunInstancedVisualizationTrait.generated.h"

class UAnimToTextureDataAsset;

USTRUCT()
struct FOverrunInstancedPresentationTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FOverrunInstanceAnimationParameters : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimToTextureDataAsset> AnimationData;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0"))
	int32 IdleAnimation = 0;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0"))
	int32 MoveAnimation = 1;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "1", Units = "cm/s"))
	float ReferenceMoveSpeed = 200.f;

	UPROPERTY(EditAnywhere, Category = "Hit", meta = (ClampMin = "0.01", Units = "s"))
	float HitFlashDuration = 0.12f;
};

USTRUCT()
struct FOverrunInstanceAnimationFragment : public FMassFragment
{
	GENERATED_BODY()

	float Phase = 0.f;
	float PreviousFrame = 0.f;
	float HitFlashRemaining = 0.f;
	uint32 LastHitSerial = 0;
	uint64 LastUpdateFrame = 0;
	bool bInitialized = false;
	bool bMoving = false;

	void Reset() { *this = FOverrunInstanceAnimationFragment(); }
};

UCLASS(meta = (DisplayName = "Overrun Instanced Visualization"))
class OVERRUN_API UOverrunInstancedVisualizationTrait : public UMassMovableVisualizationTrait
{
	GENERATED_BODY()

public:
	UOverrunInstancedVisualizationTrait();

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World,
		FAdditionalTraitRequirements& OutTraitRequirements) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Animation")
	FOverrunInstanceAnimationParameters Animation;
};
