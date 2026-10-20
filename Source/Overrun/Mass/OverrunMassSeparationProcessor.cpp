#include "OverrunMassSeparationProcessor.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassSimulationLOD.h"
#include "Movement/MassMovementProcessors.h"
#include "OverrunMassProbeTag.h"

namespace
{
	constexpr double SeparationUpdateInterval = 0.1;
	constexpr double SeparationPadding = 12.0;
	constexpr double SeparationStiffness = 120.0;
}

UOverrunMassSeparationProcessor::UOverrunMassSeparationProcessor()
	: SnapshotQuery(*this), LowQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::ApplyForces;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	ExecutionOrder.ExecuteBefore.Add(UMassApplyForceProcessor::StaticClass()->GetFName());
}

void UOverrunMassSeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SnapshotQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	SnapshotQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	SnapshotQuery.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
	SnapshotQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SnapshotQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	SnapshotQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	LowQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	LowQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::All);
	LowQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	LowQuery.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
	LowQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	LowQuery.AddConstSharedRequirement<FMassMovementParameters>();
}

void UOverrunMassSeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OverrunMassSeparation);
	const double Now = Context.GetWorld()->GetTimeSeconds();
	++Generation;
	int32 DueCount = 0;
	LowQuery.ForEachEntityChunk(Context, [this, Now, &DueCount](FMassExecutionContext& ChunkContext)
	{
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(It);
			FCache& Entry = Cache.FindOrAdd(Entity);
			if (!Entry.bInitialized)
			{
				Entry.NextUpdate = Now + double(GetTypeHash(Entity) & 65535u) / 65536.0 * SeparationUpdateInterval;
				Entry.bInitialized = true;
			}
			Entry.SeenGeneration = Generation;
			Entry.bNeedsRefresh = Now >= Entry.NextUpdate;
			DueCount += Entry.bNeedsRefresh ? 1 : 0;
		}
	});
	for (auto It = Cache.CreateIterator(); It; ++It)
	{
		if (It.Value().SeenGeneration != Generation)
		{
			It.RemoveCurrent();
		}
	}
	if (Cache.IsEmpty())
	{
		return;
	}
	if (DueCount > 0)
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
	LowQuery.ForEachEntityChunk(Context, [this, Now](FMassExecutionContext& ChunkContext)
	{
		const double MaxAcceleration = FMath::Max(0.0, double(ChunkContext.GetConstSharedFragment<FMassMovementParameters>().MaxAcceleration));
		const auto Forces = ChunkContext.GetMutableFragmentView<FMassForceFragment>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(It);
			FCache& Entry = Cache.FindChecked(Entity);
			if (Entry.bNeedsRefresh)
			{
				const int32* Index = Separation.FindIndex(Entity);
				Entry.Acceleration = Index ? Separation.CalculateAcceleration(*Index, SeparationPadding, SeparationStiffness, MaxAcceleration) : FVector::ZeroVector;
				Entry.NextUpdate = Now + SeparationUpdateInterval;
				Entry.bNeedsRefresh = false;
			}
			const FVector Correction = Entry.Acceleration.GetClampedToMaxSize(MaxAcceleration);
			if (!Correction.IsNearlyZero())
			{
				const double Remaining = FMath::Max(0.0, MaxAcceleration - Correction.Size());
				Forces[It].Value = (Correction + Forces[It].Value.GetClampedToMaxSize(Remaining)).GetClampedToMaxSize(MaxAcceleration);
			}
		}
	});
}
