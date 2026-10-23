#include "OverrunMassArrivalProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassSimulationLOD.h"
#include "Movement/MassMovementProcessors.h"
#include "OverrunCrowdArrival.h"
#include "OverrunCrowdNavigation.h"
#include "OverrunMassProbeTag.h"
#include "Steering/MassSteeringFragments.h"

UOverrunMassArrivalProcessor::UOverrunMassArrivalProcessor()
	: ArrivalQuery(*this), SnapshotQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::ApplyForces);
	ExecutionOrder.ExecuteBefore.Add(UMassApplyMovementProcessor::StaticClass()->GetFName());
}

void UOverrunMassArrivalProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ArrivalQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	ArrivalQuery.AddTagRequirement<FMassCodeDrivenMovementTag>(EMassFragmentPresence::All);
	ArrivalQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	ArrivalQuery.AddRequirement<FOverrunCrowdArrivalFragment>(EMassFragmentAccess::ReadOnly);
	ArrivalQuery.AddRequirement<FOverrunCrowdNavigationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ArrivalQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ArrivalQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddRequirement<FMassStandingSteeringFragment>(EMassFragmentAccess::ReadWrite);
	ArrivalQuery.AddConstSharedRequirement<FOverrunCrowdArrivalParameters>();
	ArrivalQuery.AddConstSharedRequirement<FMassMovementParameters>();
	SnapshotQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	SnapshotQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	SnapshotQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SnapshotQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	SnapshotQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
}

void UOverrunMassArrivalProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OverrunCrowdArrival);
	bool bNeedsSeparation = false;
	ArrivalQuery.ForEachEntityChunk(Context, [&bNeedsSeparation](FMassExecutionContext& ChunkContext)
	{
		const auto States = ChunkContext.GetFragmentView<FOverrunCrowdArrivalFragment>();
		const auto Navigation = ChunkContext.GetFragmentView<FOverrunCrowdNavigationFragment>();
		const auto& Params = ChunkContext.GetConstSharedFragment<FOverrunCrowdArrivalParameters>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if ((Navigation.IsEmpty() || Navigation[It].bFinalSegment) && States[It].bHasTarget
				&& !States[It].bHolding && States[It].DistanceToTarget < Params.CloseDistance)
			{
				bNeedsSeparation = true;
				break;
			}
		}
	});
	if (bNeedsSeparation)
	{
		Separation.Reset();
		SnapshotQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& ChunkContext)
		{
			const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
			const auto Velocities = ChunkContext.GetFragmentView<FMassVelocityFragment>();
			const auto Radii = ChunkContext.GetFragmentView<FAgentRadiusFragment>();
			for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
			{
				const FVector Position = Transforms[It].GetTransform().GetLocation();
				const FVector Velocity = Velocities[It].Value;
				if (Position.ContainsNaN() || Velocity.ContainsNaN() || !FMath::IsFinite(Radii[It].Radius))
				{
					continue;
				}
				FOverrunCrowdSeparation::FSample Sample;
				Sample.Entity = ChunkContext.GetEntity(It);
				Sample.Position = FVector2D(Position.X, Position.Y);
				Sample.Velocity = FVector2D(Velocity.X, Velocity.Y);
				Sample.Radius = FMath::Max(1.0, double(Radii[It].Radius));
				Separation.Add(Sample);
			}
		});
	}
	ArrivalQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& ChunkContext)
	{
		const auto States = ChunkContext.GetFragmentView<FOverrunCrowdArrivalFragment>();
		const auto Navigation = ChunkContext.GetFragmentView<FOverrunCrowdNavigationFragment>();
		const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const auto Velocities = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
		const auto Movement = ChunkContext.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const auto Forces = ChunkContext.GetMutableFragmentView<FMassForceFragment>();
		const auto Targets = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
		const auto Ghosts = ChunkContext.GetMutableFragmentView<FMassGhostLocationFragment>();
		const auto Standing = ChunkContext.GetMutableFragmentView<FMassStandingSteeringFragment>();
		const auto& Params = ChunkContext.GetConstSharedFragment<FOverrunCrowdArrivalParameters>();
		const auto& MoveParams = ChunkContext.GetConstSharedFragment<FMassMovementParameters>();
		const double MaxSpeed = FMath::Max(0.0, double(MoveParams.MaxSpeed));
		const double MaxAcceleration = FMath::Max(0.0, double(MoveParams.MaxAcceleration));
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const auto& State = States[It];
			const FVector Position = Transforms[It].GetTransform().GetLocation();
			if (State.bHolding || !State.bHasTarget || (!Navigation.IsEmpty() && !Navigation[It].bCanMove))
			{
				OverrunCrowd::HoldArrivalAtPosition(Position, Velocities[It], Movement[It], Forces[It],
					Targets[It], Ghosts[It], Standing[It]);
				continue;
			}
			if (State.DistanceToTarget >= Params.CloseDistance || (!Navigation.IsEmpty() && !Navigation[It].bFinalSegment))
			{
				continue;
			}
			FVector Correction = FVector::ZeroVector;
			if (const int32* Index = Separation.FindIndex(ChunkContext.GetEntity(It)))
			{
				Correction = Separation.CalculateAcceleration(*Index, Params.SeparationPadding, Params.SeparationStiffness, MaxAcceleration);
			}
			Movement[It].DesiredVelocity = OverrunCrowd::CalculateCloseArrivalVelocity(
				Velocities[It].Value, Movement[It].DesiredVelocity, State.TargetLocation - Position,
				Correction, ChunkContext.GetDeltaTimeSeconds(), Targets[It].DesiredSpeed.Get(), MaxSpeed, MaxAcceleration, Params);
		}
	});
}
