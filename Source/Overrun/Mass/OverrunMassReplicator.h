// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassReplicationProcessor.h"
#include "OverrunMassReplicator.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunMassReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()
	
public:
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
