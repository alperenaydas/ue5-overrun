// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TimerManager.h"
#include "GA_Fire.generated.h"

class UTopDownCMC;
class ATopDownCharacter;

struct FFireAbilityData
{
	FVector SampledOrigin;
	float CMCMovementTimestamp;
	FVector SampledDestination;
	double ServerReceiptTime;
};

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
	FDelegateHandle HistoryDelegateHandle;
	FTimerHandle FireTimerHandle;
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.01"))
	float FireInterval = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float MaxShotOriginError = 150.f;
	
private:
	struct FResolvedShot
	{
		FFireAbilityData Request;
		FVector ReferenceOrigin;
	};

	static constexpr double MaxPendingShotWait = 0.25;
	static constexpr float PendingShotCleanupInterval = 0.05f;
	FTimerHandle PendingShotCleanupTimerHandle;
	FTimerHandle ResolvedShotTimerHandle;

	double ServerShotCredits = 2.0; // two-shot burst capacity, refilled according to FireInterval
	double ServerLastBudgetTime = 0.0;
	bool bServerShotBudgetInitialized = false;

	bool TryConsumeServerShotBudget();
	void ExpirePendingShots();
	void OnMovementHistoryRecorded(float Timestamp);
	void ProcessResolvedShots();
	void ValidateAndApplyServerShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin);
	TWeakObjectPtr<UTopDownCMC> SubscribedCMC;

	TArray<FFireAbilityData> FireAbilityDatas;
	TArray<FResolvedShot> ResolvedShots;
};
