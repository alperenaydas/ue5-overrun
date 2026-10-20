#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassEntityHandle.h"
#include "OverrunMassCrowdLODProcessor.generated.h"

UCLASS()
class OVERRUN_API UOverrunMassCrowdLODProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UOverrunMassCrowdLODProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	struct FState
	{
		double LastModeChange = -TNumericLimits<double>::Max();
		uint64 SeenGeneration = 0;
		bool bInitialized = false;
		bool bObservedHigh = false;
	};
	FMassEntityQuery EntityQuery;
	FMassEntityQuery ConflictingLODQuery;
	TMap<FMassEntityHandle, FState> States;
	uint64 Generation = 0;
	double NextUpdateTime = 0.0;
	bool bWarnedMissingNative = false;
	bool bWarnedConflictingLOD = false;
};
