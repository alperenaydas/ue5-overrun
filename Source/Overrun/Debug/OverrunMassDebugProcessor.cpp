// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Debug/OverrunMassDebugProcessor.h"

#include "DrawDebugHelpers.h"
#include "MassActorSubsystem.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "OverrunDebugHealthWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Overrun/Mass/OverrunEnemyHealthFragment.h"
#include "Overrun/Mass/OverrunEnemyHitShapeFragment.h"

UOverrunMassDebugProcessor::UOverrunMassDebugProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void UOverrunMassDebugProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FOverrunEnemyHitShapeFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.RequireMutatingWorldAccess();
	Super::ConfigureQueries(EntityManager);
}

void UOverrunMassDebugProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	static IConsoleVariable* CapsulesCVar =
		IConsoleManager::Get().FindConsoleVariable(
			TEXT("Overrun.Debug.Capsules"));

	static IConsoleVariable* HealthCVar =
		IConsoleManager::Get().FindConsoleVariable(
			TEXT("Overrun.Debug.Health"));

	const bool bDrawCapsules =
		CapsulesCVar && CapsulesCVar->GetInt() == 1;

	const bool bDrawHealth =
		HealthCVar && HealthCVar->GetInt() == 1;

	if (!bDrawCapsules && !bDrawHealth)
	{
		return;
	}
	
	EntityQuery.ForEachEntityChunk(Context, [bDrawHealth, bDrawCapsules](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FOverrunEnemyHitShapeFragment> HitShapes = Context.GetFragmentView<FOverrunEnemyHitShapeFragment>();
		
		const TArrayView<FMassActorFragment> Actors = Context.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FOverrunEnemyHealthFragment> Healths = Context.GetFragmentView<FOverrunEnemyHealthFragment>();
		
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (bDrawCapsules && !HitShapes.IsEmpty())
			{
				const FOverrunEnemyHitShapeFragment& Shape = HitShapes[EntityIt];
				const FVector Center = Transforms[EntityIt].GetTransform().GetLocation() + FVector(0.f, 0.f, Shape.CenterZOffset);
				DrawDebugCapsule(Context.GetWorld(), Center, Shape.HalfHeight, Shape.Radius, FQuat::Identity, FColor::Yellow, false, -1.f, 0, 1.f);
			}
			if (!bDrawHealth || Actors.IsEmpty() || Healths.IsEmpty())
			{
				continue;
			}

			AActor* Actor = Actors[EntityIt].GetOwnedByMassMutable();
			if (!IsValid(Actor))
			{
				continue;
			}

			if (UWidgetComponent* View = Actor->FindComponentByClass<UWidgetComponent>())
			{
				if (UOverrunDebugHealthWidget* Widget =
					Cast<UOverrunDebugHealthWidget>(View->GetUserWidgetObject()))
				{
					Widget->SetHealthValues(Healths[EntityIt].Health, Healths[EntityIt].MaxHealth);
				}
			}
		}
	});
}
