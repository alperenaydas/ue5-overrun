// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "TopDownPlayerState.generated.h"

class UAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class OVERRUN_API ATopDownPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ATopDownPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
public:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};
