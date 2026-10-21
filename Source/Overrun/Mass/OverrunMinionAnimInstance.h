// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OverrunMinionAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunMinionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetPresentationGroundSpeed(float InGroundSpeed);

protected:
	virtual void NativeInitializeAnimation() override;
	
private:
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed = 0.f;
};
