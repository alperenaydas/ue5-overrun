// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Overrun/AbilitySystem/OverrunAttributeSet.h"
#include "Overrun/Character/TopDownCharacter.h"

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

void ATopDownPlayerState::SetDeadState(bool isDead)
{
	if (!HasAuthority() || IsDead == isDead) return;
	
	IsDead = isDead;
	if (isDead)
	{
		CharacterEnterDeadTransition();
	}
	else
	{
		//respawn logic
	}
}

void ATopDownPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATopDownPlayerState, IsDead);
	
}

void ATopDownPlayerState::OnRep_IsDead(bool bOldIsDead) const
{
	UE_LOG(LogPlayerManagement, Display, TEXT("bOldIsDead: %hhd, IsDead: %hhd"), bOldIsDead, IsDead);
	if (bOldIsDead == IsDead) return;
	if (IsDead)
	{
		CharacterEnterDeadTransition();
	}
	else
	{
		//respawn logic
	}
}

void ATopDownPlayerState::CharacterEnterDeadTransition() const
{
	if (ATopDownCharacter* Character = Cast<ATopDownCharacter>(GetPawn()))
	{
		Character->EnterDeadTransition();
	}
}
