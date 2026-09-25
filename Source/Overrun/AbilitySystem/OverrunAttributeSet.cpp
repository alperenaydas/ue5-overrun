// Fill out your copyright notice in the Description page of Project Settings.


#include "OverrunAttributeSet.h"

#include "AbilitySystemLog.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Overrun/Player/TopDownPlayerState.h"

void UOverrunAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UOverrunAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOverrunAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOverrunAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UOverrunAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	ClampAttributes(Attribute, NewValue);
}

void UOverrunAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	
	ClampAttributes(Attribute, NewValue);
}

bool UOverrunAttributeSet::PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		if (ATopDownPlayerState* GEOwnerPlayerState = Cast<ATopDownPlayerState>(Data.Target.GetOwner()))
		{
			if (GEOwnerPlayerState->IsDead)
			{
				UE_LOG(LogAbilitySystem, Log, TEXT("Player with id: %d is already dead. GameplayEffect is not executable."), GEOwnerPlayerState->GetPlayerId());
				return false;
			}
		}
	}
	
	return Super::PreGameplayEffectExecute(Data);
}

void UOverrunAttributeSet::ClampAttributes(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		// health can't go negative or above max health
		NewValue = FMath::Clamp(NewValue, .0f, GetMaxHealth());
	}
	
	if (Attribute == GetMaxHealthAttribute())
	{
		// max health can't be lower than 1
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UOverrunAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		UE_LOG(LogTemp, Display, TEXT("%s health effect executed. New health: %.1f, Max health: %.1f"), *GetNameSafe(GetOwningActor()), GetHealth(), GetMaxHealth());
	}
	
}

void UOverrunAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOverrunAttributeSet, Stamina, OldStamina);
}

void UOverrunAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOverrunAttributeSet, Health, OldHealth);
}

void UOverrunAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOverrunAttributeSet, MaxHealth, OldMaxHealth);
}
