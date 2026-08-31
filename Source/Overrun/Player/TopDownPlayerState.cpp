// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownPlayerState.h"

#include "AbilitySystemComponent.h"

ATopDownPlayerState::ATopDownPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);
}

UAbilitySystemComponent* ATopDownPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
