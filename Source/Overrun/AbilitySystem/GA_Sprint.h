// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OverrunMovementAbility.h"
#include "GA_Sprint.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UGA_Sprint : public UOverrunMovementAbility
{
	GENERATED_BODY()
public:
	UGA_Sprint();
	
protected:
	virtual void ActivateMovementAbility(UTopDownCMC* CMC) override;
};
