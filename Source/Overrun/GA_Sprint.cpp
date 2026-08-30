// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Sprint.h"

#include "AbilitySystemLog.h"
#include "OverrunNetDebug.h"
#include "TopDownCharacter.h"
#include "TopDownCMC.h"

UGA_Sprint::UGA_Sprint()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{			
		constexpr bool bReplicateEndAbility = true;
		constexpr bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	TWeakObjectPtr<AActor> Avatar = ActorInfo->AvatarActor;
	if (ATopDownCharacter* TopDownCharacter = Cast<ATopDownCharacter>(Avatar.Get()))
	{
		if (UTopDownCMC* CMC = Cast<UTopDownCMC>(TopDownCharacter->GetMovementComponent()))
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogAbilitySystem, Log, TEXT("Starting sprinting with activation mode: %s"), *GetActivationModeName(ActivationInfo.ActivationMode));
#endif
			CMC->SetSprinting(true);
			constexpr bool bReplicateEndAbility = true;
			constexpr bool bWasCancelled = false;
			EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
			return;
		}
	}
	UE_LOG(LogAbilitySystem, Warning, TEXT("Couldn't cast the Avatar to TopDownCharacter, or CMC. Ability Cancelled."))
	constexpr bool bReplicateEndAbility = true;
	constexpr bool bWasCancelled = true;
	EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
