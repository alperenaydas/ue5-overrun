// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "OverrunMovementAbility.generated.h"

class UTopDownCMC;
/**
 * 
 */
UCLASS(Abstract)
class OVERRUN_API UOverrunMovementAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UOverrunMovementAbility();
	
protected:
	virtual void ActivateMovementAbility(UTopDownCMC* CMC) PURE_VIRTUAL(UOverrunMovementAbility::ActivateMovementAbility, );
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
								 const FGameplayAbilityActivationInfo ActivationInfo,
								 const FGameplayEventData* TriggerEventData) override;
};
