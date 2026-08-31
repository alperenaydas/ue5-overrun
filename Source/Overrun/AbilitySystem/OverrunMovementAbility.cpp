// Fill out your copyright notice in the Description page of Project Settings.


#include "OverrunMovementAbility.h"

#include "AbilitySystemLog.h"
#include "../Debug/OverrunNetDebug.h"
#include "../Character/TopDownCharacter.h"
#include "../Movement/TopDownCMC.h"


UOverrunMovementAbility::UOverrunMovementAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Set tag on child class.
}

void UOverrunMovementAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                              const FGameplayAbilityActivationInfo ActivationInfo,
                                              const FGameplayEventData* TriggerEventData)
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
			UE_LOG(LogAbilitySystem, Log, TEXT("Starting %s with activation mode: %s"),
			       *GetName(),
			       *GetActivationModeName(ActivationInfo.ActivationMode));
#endif
			ActivateMovementAbility(CMC);
			constexpr bool bReplicateEndAbility = true;
			constexpr bool bWasCancelled = false;
			EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
			return;
		}
	}
	UE_LOG(LogAbilitySystem, Warning, TEXT("Couldn't cast the Avatar to TopDownCharacter, or CMC. Ability Cancelled."));
	constexpr bool bReplicateEndAbility = true;
	constexpr bool bWasCancelled = true;
	EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

