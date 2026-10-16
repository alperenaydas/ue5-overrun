// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "OverrunMassRepRemovalObserver.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunMassRepRemovalObserver : public UMassObserverProcessor
{
	GENERATED_BODY()
	
public:
	UOverrunMassRepRemovalObserver();
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	FMassEntityQuery EntityQuery;
};
