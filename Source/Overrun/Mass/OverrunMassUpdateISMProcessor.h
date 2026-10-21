#pragma once

#include "CoreMinimal.h"
#include "MassUpdateISMProcessor.h"
#include "OverrunMassUpdateISMProcessor.generated.h"

UCLASS()
class OVERRUN_API UOverrunMassUpdateISMProcessor : public UMassUpdateISMProcessor
{
	GENERATED_BODY()

public:
	UOverrunMassUpdateISMProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery InstancedQuery;
};
