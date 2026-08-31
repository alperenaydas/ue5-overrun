// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Overrun/AbilitySystem/OverrunAttributeSet.h"

ATopDownPlayerState::ATopDownPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);
	
	OverrunAttributeSet = CreateDefaultSubobject<UOverrunAttributeSet>(TEXT("OverrunAttributeSet"));
}

UAbilitySystemComponent* ATopDownPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
