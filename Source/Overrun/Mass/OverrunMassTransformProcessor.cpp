// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunMassTransformProcessor.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "GameFramework/Actor.h"
#include "OverrunMassProbeTag.h"
#include "Engine/World.h"

UOverrunMassTransformProcessor::UOverrunMassTransformProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::FrameEnd;
}

void UOverrunMassTransformProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RequireMutatingWorldAccess();
	Super::ConfigureQueries(EntityManager);
}

void UOverrunMassTransformProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();

		const TArrayView<FMassActorFragment> Actors = Context.GetMutableFragmentView<FMassActorFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (AActor* Actor = Actors[EntityIt].GetOwnedByMassMutable())
			{
				FTransform TargetTransform = Transforms[EntityIt].GetTransform();
				if (Context.GetWorld()->GetNetMode() == NM_Client)
				{
					TargetTransform.SetLocation(FMath::VInterpTo(
						Actor->GetActorLocation(),
						TargetTransform.GetLocation(),
						Context.GetDeltaTimeSeconds(),
						10.f));

					TargetTransform.SetRotation(FMath::RInterpTo(
						Actor->GetActorRotation(),
						TargetTransform.Rotator(),
						Context.GetDeltaTimeSeconds(),
						10.f).Quaternion());
				}
				
				Actor->SetActorTransform(
					TargetTransform,
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
			}
		}
	});
}
