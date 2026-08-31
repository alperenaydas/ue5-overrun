// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OverrunAnimInstance.generated.h"

class ATopDownCharacter;
/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
private:
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float LocomotionDirection = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bIsMoving = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float PlayRate = 1.f;
	
	UPROPERTY()
	TObjectPtr<ATopDownCharacter> OwningCharacter = nullptr;
};
