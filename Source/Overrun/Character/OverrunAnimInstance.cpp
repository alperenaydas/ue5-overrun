// Fill out your copyright notice in the Description page of Project Settings.


#include "OverrunAnimInstance.h"

#include "TopDownCharacter.h"
#include "../Movement/TopDownCMC.h"

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
		bIsMoving = GroundSpeed > 3.f;
		if (bIsMoving)
		{
			LocomotionDirection = OwningCharacter->GetLocomotionDirection();
		}

		const float TargetPlayRate = GroundSpeed / OwningCharacter->GetCharacterMovement()->MaxWalkSpeed; 
		PlayRate = FMath::Clamp(TargetPlayRate, 0.25f, 2.5f);
	}
}
