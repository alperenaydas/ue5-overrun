#include "Overrun/Mass/OverrunMassMovementProcessor.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "OverrunMassProbeTag.h"
#include "Overrun/Character/TopDownCharacter.h"
#include "Overrun/Player/TopDownPlayerState.h"

UOverrunMassMovementProcessor::UOverrunMassMovementProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);

	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void UOverrunMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	Super::ConfigureQueries(EntityManager);
}

void UOverrunMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	TArray<FVector> PlayerLocations;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
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

	EntityQuery.ForEachEntityChunk(Context, [&PlayerLocations, World](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> Velocities = Context.GetFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargets = Context.GetMutableFragmentView<FMassMoveTargetFragment>();

		constexpr float DesiredSpeed = 300.f;
		constexpr double StopDistance = 100.0;
		constexpr double ResumeDistance = 150.0;
		constexpr float TurnRate = 360.f;

		const float DeltaSeconds = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			FMassMoveTargetFragment& MoveTarget = MoveTargets[EntityIt];

			const FVector CurrentLocation = Transform.GetLocation();

			FVector ToPlayer = FVector::ZeroVector;
			double DistanceToPlayer = 0.0;

			if (!PlayerLocations.IsEmpty())
			{
				FVector ClosestPlayer = PlayerLocations[0];
				double ClosestDistanceSquared = TNumericLimits<double>::Max();

				for (const FVector& PlayerLocation : PlayerLocations)
				{
					const double DistanceSquared = (PlayerLocation - CurrentLocation).SizeSquared2D();

					if (DistanceSquared < ClosestDistanceSquared)
					{
						ClosestDistanceSquared = DistanceSquared;
						ClosestPlayer = PlayerLocation;
					}
				}

				ToPlayer = ClosestPlayer - CurrentLocation;
				ToPlayer.Z = 0.0;
				DistanceToPlayer = ToPlayer.Size();
			}

			const FVector ToPlayerDirection = ToPlayer.GetSafeNormal();

			const bool bWasStanding = MoveTarget.GetCurrentAction() == EMassMovementAction::Stand;

			const double MoveThreshold = bWasStanding ? ResumeDistance : StopDistance;

			const bool bShouldMove = !PlayerLocations.IsEmpty() && DistanceToPlayer > MoveThreshold;

			const EMassMovementAction WantedAction = bShouldMove ? EMassMovementAction::Move : EMassMovementAction::Stand;

			const bool bStartingAction = MoveTarget.GetCurrentAction() != WantedAction || MoveTarget.GetCurrentActionID() == 0;

			if (bStartingAction)
			{
				MoveTarget.CreateNewAction(WantedAction, *World);
			}

			MoveTarget.DesiredSpeed.Set(DesiredSpeed);
			MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
			MoveTarget.SlackRadius = 0.f;
			MoveTarget.EntityDistanceToGoal = FMassMoveTargetFragment::UnsetDistance;

			if (bShouldMove)
			{
				const double RemainingDistance = FMath::Max(DistanceToPlayer - StopDistance, 0.0);

				MoveTarget.Center = CurrentLocation + ToPlayerDirection * RemainingDistance;

				MoveTarget.Forward = ToPlayerDirection;
				MoveTarget.DistanceToGoal = static_cast<float>(RemainingDistance);
			}
			else
			{
				if (bStartingAction)
				{
					MoveTarget.Center = CurrentLocation;
					MoveTarget.Forward = Transform.GetRotation().GetForwardVector();
				}

				MoveTarget.DistanceToGoal = 0.f;
			}

			FVector FacingDirection = Velocities[EntityIt].Value;
			FacingDirection.Z = 0.0;

			if (FacingDirection.SizeSquared() < FMath::Square(5.0))
			{
				FacingDirection = ToPlayerDirection;
			}

			if (!FacingDirection.IsNearlyZero())
			{
				const FRotator NewRotation =
					FMath::RInterpConstantTo(
						Transform.GetRotation().Rotator(),
						FacingDirection.Rotation(),
						DeltaSeconds,
						TurnRate);

				Transform.SetRotation(NewRotation.Quaternion());
			}
		}
	});
}
