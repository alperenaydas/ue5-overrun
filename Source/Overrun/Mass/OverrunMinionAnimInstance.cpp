// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Mass/OverrunMinionAnimInstance.h"
#include "GameFramework/Actor.h"

void UOverrunMinionAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	GroundSpeed = 0.f;
	bHasPreviousLocation = false;
}

void UOverrunMinionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	AActor* OwningActor = GetOwningActor();
	if (OwningActor && !FMath::IsNearlyZero(DeltaSeconds))
	{
		FVector CurrentLocation = OwningActor->GetActorLocation();
		if (!bHasPreviousLocation)
		{
			PreviousLocation = CurrentLocation;
			bHasPreviousLocation = true;
		}
		else
		{
			GroundSpeed = static_cast<float>((CurrentLocation - PreviousLocation).Size2D()) / DeltaSeconds;
			PreviousLocation = CurrentLocation;
		}
	}
	else
	{
		GroundSpeed = 0.f;
		bHasPreviousLocation = false;
	}
}
