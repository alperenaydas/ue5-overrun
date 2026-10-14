// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunMassMovementProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "OverrunMassProbeTag.h"

UOverrunMassMovementProcessor::UOverrunMassMovementProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
}

void UOverrunMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	Super::ConfigureQueries(EntityManager);
}

void UOverrunMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, ([this](FMassExecutionContext& Context)
		{
			const TArrayView<FTransformFragment> TransformsList = Context.GetMutableFragmentView<FTransformFragment>();
			const float WorldDeltaTime = Context.GetDeltaTimeSeconds();
			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FVector Displacement = FVector(100.f, 0.f, 0.f) * WorldDeltaTime;
				FTransform& Transform = TransformsList[EntityIt].GetMutableTransform();
				Transform.SetTranslation(Transform.GetTranslation() + Displacement);
			}
		}));
}
