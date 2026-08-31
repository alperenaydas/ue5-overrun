// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OverrunMovementAbility.h"
#include "GA_Dash.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UGA_Dash : public UOverrunMovementAbility
{
	GENERATED_BODY()
public:
	UGA_Dash();

protected:
	virtual void ActivateMovementAbility(UTopDownCMC* CMC) override;
};
