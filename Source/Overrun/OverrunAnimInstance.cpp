// Fill out your copyright notice in the Description page of Project Settings.


#include "OverrunAnimInstance.h"

#include "TopDownCharacter.h"

void UOverrunAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwningCharacter = Cast<ATopDownCharacter>(TryGetPawnOwner());	
}

void UOverrunAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (OwningCharacter)
	{
		GroundSpeed = OwningCharacter->GetGroundSpeed();
		LocomotionDirection = OwningCharacter->GetLocomotionDirection();
		bIsMoving = GroundSpeed > 3.f;
	}
}
