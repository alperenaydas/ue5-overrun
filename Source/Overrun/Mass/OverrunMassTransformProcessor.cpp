// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunMassTransformProcessor.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "OverrunEnemyHealthFragment.h"
#include "OverrunEnemyHitPresentationFragment.h"
#include "GameFramework/Actor.h"
#include "OverrunMassProbeTag.h"
#include "Engine/World.h"
#include "Overrun/Combat/OverrunHitFlashComponent.h"

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
	EntityQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FOverrunEnemyHitPresentationFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.RequireMutatingWorldAccess();
}

void UOverrunMassTransformProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const bool bCanPresent = Context.GetWorld()->GetNetMode() != NM_DedicatedServer;
	EntityQuery.ForEachEntityChunk(Context, [bCanPresent](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassActorFragment> Actors = Context.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FOverrunEnemyHealthFragment> Healths = Context.GetFragmentView<FOverrunEnemyHealthFragment>();
		const TArrayView<FOverrunEnemyHitPresentationFragment> Presentations = Context.GetMutableFragmentView<FOverrunEnemyHitPresentationFragment>();

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
				
				if (!bCanPresent || Healths.IsEmpty() || Presentations.IsEmpty())
				{
					continue;
				}

				FOverrunEnemyHitPresentationFragment& Presentation = Presentations[EntityIt];

				const uint32 HitSerial = Healths[EntityIt].HitSerial;

				if (Presentation.VisualActor.Get() != Actor)
				{
					Presentation.VisualActor = Actor;
					Presentation.FlashComponent = Actor->FindComponentByClass<UOverrunHitFlashComponent>();
					Presentation.LastHitSerial = HitSerial;
				}
				else if (Presentation.LastHitSerial != HitSerial)
				{
					Presentation.LastHitSerial = HitSerial;

					if (UOverrunHitFlashComponent* Flash = Presentation.FlashComponent.Get())
					{
						Flash->PlayHitFlash();
					}
				}
			}
		}
	});
}
