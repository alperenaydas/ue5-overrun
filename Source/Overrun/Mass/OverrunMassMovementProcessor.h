// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "OverrunMassMovementProcessor.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UOverrunMassMovementProcessor();
		
protected:	
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
