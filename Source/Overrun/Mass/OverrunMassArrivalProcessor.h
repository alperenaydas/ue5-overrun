#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "OverrunCrowdSeparation.h"
#include "OverrunMassArrivalProcessor.generated.h"

UCLASS()
class OVERRUN_API UOverrunMassArrivalProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UOverrunMassArrivalProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ArrivalQuery;
	FMassEntityQuery SnapshotQuery;
	FOverrunCrowdSeparation Separation;
};
