// Fill out your copyright notice in the Description page of Project Settings.

#include "OverrunMassTransformProcessor.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassNavigationProcessors.h"
#include "MassRepresentationFragments.h"
#include "OverrunEnemyHealthFragment.h"
#include "OverrunEnemyHitPresentationFragment.h"
#include "OverrunEnemyPresentationFragment.h"
#include "OverrunMassProbeTag.h"
#include "OverrunMinionAnimInstance.h"
#include "Overrun/Combat/OverrunHitFlashComponent.h"
#include "Translators/MassCapsuleComponentTranslators.h"
#include "Translators/MassCharacterMovementTranslators.h"
#include "Translators/MassSceneComponentLocationTranslator.h"

namespace
{
	constexpr float ClientInterpolationSpeed = 10.f;
	constexpr double TeleportDistanceSquared = 1000.0 * 1000.0;
}

UOverrunMassTransformProcessor::UOverrunMassTransformProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	ExecutionOrder.ExecuteAfter.Add(UMassNavigationSmoothHeightProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void UOverrunMassTransformProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassCapsuleTransformCopyToActorTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassCharacterMovementCopyToActorTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassCharacterOrientationCopyToActorTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassSceneComponentLocationCopyToActorTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FOverrunEnemyPresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FOverrunEnemyHitPresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RequireMutatingWorldAccess();
}

void UOverrunMassTransformProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bSmooth = World->GetNetMode() == NM_Client;
	EntityQuery.ForEachEntityChunk(Context, [bSmooth](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassRepresentationFragment> Representations = Context.GetFragmentView<FMassRepresentationFragment>();
		const TArrayView<FMassActorFragment> Actors = Context.GetMutableFragmentView<FMassActorFragment>();
		const TArrayView<FOverrunEnemyPresentationFragment> Poses = Context.GetMutableFragmentView<FOverrunEnemyPresentationFragment>();
		const TConstArrayView<FOverrunEnemyHealthFragment> Healths = Context.GetFragmentView<FOverrunEnemyHealthFragment>();
		const TArrayView<FOverrunEnemyHitPresentationFragment> Hits = Context.GetMutableFragmentView<FOverrunEnemyHitPresentationFragment>();
		const float DeltaSeconds = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FOverrunEnemyPresentationFragment& Pose = Poses[EntityIt];
			FOverrunEnemyHitPresentationFragment& Hit = Hits[EntityIt];
			const EMassRepresentationType Representation = Representations[EntityIt].CurrentRepresentation;
			const bool bActorRepresentation = Representation == EMassRepresentationType::HighResSpawnedActor
				|| Representation == EMassRepresentationType::LowResSpawnedActor;
			AActor* Actor = bActorRepresentation ? Actors[EntityIt].GetOwnedByMassMutable() : nullptr;
			if (Representation == EMassRepresentationType::None)
			{
				Pose.Reset();
				Hit = FOverrunEnemyHitPresentationFragment();
				continue;
			}

			const FTransform& Target = Transforms[EntityIt].GetTransform();
			const bool bActorChanged = Pose.VisualActor.Get() != Actor;
			const bool bReset = !Pose.bInitialized
				|| Pose.LastUpdateFrame + 1 != GFrameCounter
				|| FVector::DistSquared(Pose.PreviousTargetLocation, Target.GetLocation()) > TeleportDistanceSquared;
			const FVector PreviousDisplayLocation = Pose.DisplayTransform.GetLocation();
			if (bReset || !bSmooth)
			{
				Pose.DisplayTransform = Target;
			}
			else
			{
				Pose.DisplayTransform.SetLocation(FMath::VInterpTo(
					PreviousDisplayLocation, Target.GetLocation(), DeltaSeconds, ClientInterpolationSpeed));
				Pose.DisplayTransform.SetRotation(FMath::RInterpTo(
					Pose.DisplayTransform.Rotator(), Target.Rotator(), DeltaSeconds, ClientInterpolationSpeed).Quaternion());
				Pose.DisplayTransform.SetScale3D(Target.GetScale3D());
			}
			Pose.GroundSpeed = !bReset && DeltaSeconds > UE_SMALL_NUMBER
				? static_cast<float>((Pose.DisplayTransform.GetLocation() - PreviousDisplayLocation).Size2D()) / DeltaSeconds
				: 0.f;
			Pose.PreviousTargetLocation = Target.GetLocation();
			Pose.LastUpdateFrame = GFrameCounter;
			Pose.bInitialized = true;
			Pose.bResetThisFrame = bReset;

			if (bActorChanged)
			{
				Pose.VisualActor = Actor;
				Pose.SkeletalMesh = Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
			}
			if (!Actor)
			{
				Hit = FOverrunEnemyHitPresentationFragment();
				continue;
			}

			Actor->SetActorTransform(Pose.DisplayTransform, false, nullptr, ETeleportType::TeleportPhysics);
			if (USkeletalMeshComponent* Mesh = Pose.SkeletalMesh.Get())
			{
				if (UOverrunMinionAnimInstance* Animation = Cast<UOverrunMinionAnimInstance>(Mesh->GetAnimInstance()))
				{
					Animation->SetPresentationGroundSpeed(Pose.GroundSpeed);
				}
			}

			if (Healths.IsEmpty())
			{
				continue;
			}
			const uint32 HitSerial = Healths[EntityIt].HitSerial;
			if (Hit.VisualActor.Get() != Actor)
			{
				Hit.VisualActor = Actor;
				Hit.FlashComponent = Actor->FindComponentByClass<UOverrunHitFlashComponent>();
				Hit.LastHitSerial = HitSerial;
			}
			else if (Hit.LastHitSerial != HitSerial)
			{
				Hit.LastHitSerial = HitSerial;
				if (UOverrunHitFlashComponent* Flash = Hit.FlashComponent.Get())
				{
					Flash->PlayHitFlash();
				}
			}
		}
	});
}
