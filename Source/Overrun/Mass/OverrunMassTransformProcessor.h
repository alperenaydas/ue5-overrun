// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "OverrunMassTransformProcessor.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunMassTransformProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
public:
	UOverrunMassTransformProcessor();
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};
