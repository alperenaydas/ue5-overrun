// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Overrun/Combat/OverrunServerShotTypes.h"
#include "TimerManager.h"
#include "OverrunServerShotComponent.generated.h"


class UTopDownCMC;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OVERRUN_API UOverrunServerShotComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UOverrunServerShotComponent();
	void SubmitShot(const FFireAbilityData& Shot);
	void CancelPendingShots();

	int32 GetNumQueuedShots() const
	{
		return PendingHistoryShots.Num() + ResolvedShots.Num();
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	struct FResolvedShot
	{
		FFireAbilityData Request;
		FVector ReferenceOrigin;
	};
	TArray<FResolvedShot> ResolvedShots;
	FTimerHandle ResolvedShotTimerHandle;
	
	TArray<FFireAbilityData> PendingHistoryShots;
	FTimerHandle PendingShotCleanupTimerHandle;
	TWeakObjectPtr<UTopDownCMC> SubscribedCMC;
	FDelegateHandle HistoryDelegateHandle;
	
	static constexpr float PendingShotCleanupInterval = 0.05f;



private:
	void ExpirePendingShots();
	void ProcessResolvedShots();
	void OnMovementHistoryRecorded(float Timestamp);
	void ValidateAndApplyServerShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin);
	void QueueResolvedShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin);
		
};
