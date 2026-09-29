// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TimerManager.h"
#include "GA_Fire.generated.h"

class ATopDownCharacter;
/**
 * 
 */
UCLASS()
class OVERRUN_API UGA_Fire : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_Fire();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	void FireLocalShot();
	void OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	FDelegateHandle TargetDataDelegateHandle;
	FTimerHandle FireTimerHandle;
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.01"))
	float FireInterval = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float MaxShotOriginError = 150.f;
	
private:
	double ServerShotCredits = 2.0; // with 0.1 fire interval, 200ms credit budget
	double ServerLastBudgetTime = 0.0;
	bool bServerShotBudgetInitialized = false;

	bool TryConsumeServerShotBudget();
};
