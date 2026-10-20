#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "OverrunCrowdSeparation.h"
#include "OverrunMassSeparationProcessor.generated.h"

UCLASS()
class OVERRUN_API UOverrunMassSeparationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UOverrunMassSeparationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	struct FCache
	{
		FVector Acceleration = FVector::ZeroVector;
		double NextUpdate = 0.0;
		uint64 SeenGeneration = 0;
		bool bInitialized = false;
		bool bNeedsRefresh = false;
	};
	FMassEntityQuery SnapshotQuery;
	FMassEntityQuery LowQuery;
	FOverrunCrowdSeparation Separation;
	TMap<FMassEntityHandle, FCache> Cache;
	uint64 Generation = 0;
};
