#pragma once

#include "CoreMinimal.h"
#include "MassNavMeshNavigationProcessors.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "OverrunMassNavigationProcessor.generated.h"

UCLASS()
class OVERRUN_API UOverrunMassNavigationProcessor : public UMassNavMeshPathFollowProcessor
{
	GENERATED_BODY()

public:
	UOverrunMassNavigationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery NavigationQuery;
	bool bWarnedMissingNavData = false;
};

UCLASS()
class OVERRUN_API UOverrunMassNavigationConstraintProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UOverrunMassNavigationConstraintProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery ConstraintQuery;
};
