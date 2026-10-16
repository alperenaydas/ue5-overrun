// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunMassMovementProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "OverrunMassProbeTag.h"
#include "Overrun/Character/TopDownCharacter.h"
#include "Overrun/Player/TopDownPlayerState.h"

UOverrunMassMovementProcessor::UOverrunMassMovementProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
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
		TArray<FVector> PlayerLocations;
		for (FConstPlayerControllerIterator It = Context.GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (!IsValid(Controller))
			{
				continue;
			}
			ATopDownCharacter* Character = Controller->GetPawn<ATopDownCharacter>();
			if (!IsValid(Character))
			{
				continue;
			}
			const ATopDownPlayerState* PlayerState = Character->GetPlayerState<ATopDownPlayerState>();
			if (!IsValid(PlayerState) || Character->IsDead())
			{
				continue;
			}
			PlayerLocations.Add(Character->GetActorLocation());
		}
		if (PlayerLocations.IsEmpty())
		{
			return;
		}
		const TArrayView<FTransformFragment> TransformsList = Context.GetMutableFragmentView<FTransformFragment>();
		const double WorldDeltaTime = Context.GetDeltaTimeSeconds();
		constexpr double MoveSpeed = 300.0;
		constexpr double StopDistance = 100.0;
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FTransform& Transform = TransformsList[EntityIt].GetMutableTransform();
			const FVector CurrentLocation = Transform.GetLocation();
			double ClosestDistanceSquared = TNumericLimits<double>::Max();
			FVector TargetLocation;
			for (const FVector& PlayerLocation : PlayerLocations)
			{
				const double DistanceSquared = (PlayerLocation - CurrentLocation).SizeSquared2D();

				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					TargetLocation = PlayerLocation;
				}
			}
			FVector ToTarget = TargetLocation - CurrentLocation;
			ToTarget.Z = 0.0;
			const double Distance = ToTarget.Size();
			if (Distance <= UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector MoveDirection = ToTarget / Distance;
			const double StepDistance = FMath::Min(MoveSpeed * WorldDeltaTime,FMath::Max(Distance - StopDistance, 0.0));
			Transform.SetTranslation(CurrentLocation + MoveDirection * StepDistance);
			constexpr float TurnRate = 360.f;

			const FRotator NewRotation = FMath::RInterpConstantTo(
				Transform.GetRotation().Rotator(),
				MoveDirection.Rotation(),
				static_cast<float>(WorldDeltaTime),
				TurnRate);

			Transform.SetRotation(NewRotation.Quaternion());
		}
	}));
}
