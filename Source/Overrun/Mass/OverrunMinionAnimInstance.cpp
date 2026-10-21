// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Mass/OverrunMinionAnimInstance.h"

void UOverrunMinionAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	GroundSpeed = 0.f;
}

void UOverrunMinionAnimInstance::SetPresentationGroundSpeed(float InGroundSpeed)
{
	check(IsInGameThread());
	GroundSpeed = FMath::IsFinite(InGroundSpeed) ? FMath::Max(0.f, InGroundSpeed) : 0.f;
}
