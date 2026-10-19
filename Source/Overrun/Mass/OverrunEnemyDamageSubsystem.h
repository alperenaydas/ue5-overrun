// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "OverrunEnemyDamageCommand.h"
#include "Subsystems/WorldSubsystem.h"
#include "OverrunEnemyDamageSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunEnemyDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	bool EnqueueDamage(const FOverrunEnemyDamageCommand& Command);
	TArray<FOverrunEnemyDamageCommand> ConsumePendingDamage();
	bool FindClosestEnemyHit(const FVector& Start, const FVector& UnitDirection, float MaxDistance, FMassEntityHandle& OutEntity, float& OutDistance);
	
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	
private:
	bool CanAccessQueue() const;
	
	static constexpr int32 MaxPendingCommands = 256;
	TArray<FOverrunEnemyDamageCommand> PendingCommands;
	
	FMassEntityQuery EnemyHitQuery;
};
