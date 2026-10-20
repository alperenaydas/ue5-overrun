#include "OverrunMassCrowdLODProcessor.h"

#include "Avoidance/MassAvoidanceFragments.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h"
#include "MassSimulationLOD.h"
#include "OverrunCrowdLOD.h"
#include "OverrunMassProbeTag.h"
#include "Overrun/Character/TopDownCharacter.h"
#include "Overrun/Player/TopDownPlayerState.h"

namespace
{
	constexpr double LODUpdateInterval = 0.2;
}

UOverrunMassCrowdLODProcessor::UOverrunMassCrowdLODProcessor()
	: EntityQuery(*this), ConflictingLODQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LODCollector);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Tasks);
}

void UOverrunMassCrowdLODProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FMassNavigationEdgesFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavigationObstacleGridCellLocationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FMassMovingAvoidanceParameters>(EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FMassStandingAvoidanceParameters>(EMassFragmentPresence::Optional);
	ConflictingLODQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	ConflictingLODQuery.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly);
}

void UOverrunMassCrowdLODProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OverrunCrowdLOD);
	UWorld* World = Context.GetWorld();
	const double Now = World->GetTimeSeconds();
	if (Now < NextUpdateTime)
	{
		return;
	}
	NextUpdateTime = Now + LODUpdateInterval;
	++Generation;

	ConflictingLODQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& ChunkContext)
	{
		if (!bWarnedConflictingLOD && ChunkContext.GetNumEntities() > 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Overrun crowd entities cannot use the Simulation LOD trait; the crowd processor owns simulation LOD tags. These entities will be skipped."));
			bWarnedConflictingLOD = true;
		}
	});

	TArray<FVector> PlayerLocations;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const ATopDownCharacter* Character = IsValid(Controller) ? Controller->GetPawn<ATopDownCharacter>() : nullptr;
		if (IsValid(Character) && IsValid(Character->GetPlayerState<ATopDownPlayerState>()) && !Character->IsDead())
		{
			const FVector Position = Character->GetActorLocation();
			if (!Position.ContainsNaN())
			{
				PlayerLocations.Add(Position);
			}
		}
	}

	TArray<OverrunCrowd::FLODCandidate> Candidates;
	int32 MissingNative = 0;
	EntityQuery.ForEachEntityChunk(Context, [this, Now, &PlayerLocations, &Candidates, &MissingNative](FMassExecutionContext& ChunkContext)
	{
		const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const bool bHasNative = ChunkContext.GetConstSharedFragmentPtr<FMassMovingAvoidanceParameters>() &&
			ChunkContext.GetConstSharedFragmentPtr<FMassStandingAvoidanceParameters>() &&
			!ChunkContext.GetFragmentView<FMassNavigationEdgesFragment>().IsEmpty() &&
			!ChunkContext.GetFragmentView<FMassNavigationObstacleGridCellLocationFragment>().IsEmpty();
		const bool bLow = ChunkContext.DoesArchetypeHaveTag<FMassLowLODTag>();
		const bool bMedium = ChunkContext.DoesArchetypeHaveTag<FMassMediumLODTag>();
		const bool bOff = ChunkContext.DoesArchetypeHaveTag<FMassOffLODTag>();
		const bool bHighTag = ChunkContext.DoesArchetypeHaveTag<FMassHighLODTag>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			OverrunCrowd::FLODCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Entity = ChunkContext.GetEntity(It);
			Candidate.bHasNativeAvoidance = bHasNative;
			Candidate.bWasHigh = bHasNative && !bLow && !bMedium && !bOff;
			Candidate.bHadHighTag = bHighTag;
			Candidate.bHadLowTag = bLow;
			Candidate.bHadMediumTag = bMedium;
			Candidate.bHadOffTag = bOff;
			MissingNative += bHasNative ? 0 : 1;
			FState& State = States.FindOrAdd(Candidate.Entity);
			if (State.bInitialized && State.bObservedHigh != Candidate.bWasHigh)
			{
				State.LastModeChange = Now;
			}
			State.bInitialized = true;
			State.bObservedHigh = Candidate.bWasHigh;
			State.SeenGeneration = Generation;
			Candidate.LastModeChange = State.LastModeChange;
			const FVector Position = Transforms[It].GetTransform().GetLocation();
			if (Position.ContainsNaN())
			{
				continue;
			}
			for (int32 Player = 0; Player < PlayerLocations.Num(); ++Player)
			{
				const double Distance = FVector::Dist2D(Position, PlayerLocations[Player]);
				if (Distance < Candidate.Distance)
				{
					Candidate.Distance = Distance;
					Candidate.PlayerIndex = Player;
				}
			}
		}
	});
	for (auto It = States.CreateIterator(); It; ++It)
	{
		if (It.Value().SeenGeneration != Generation)
		{
			It.RemoveCurrent();
		}
	}
	if (MissingNative > 0 && !bWarnedMissingNative)
	{
		UE_LOG(LogTemp, Warning, TEXT("%d Overrun crowd entities lack Avoidance or Navigation Obstacle data and will use separation only."), MissingNative);
		bWarnedMissingNative = true;
	}

	const OverrunCrowd::FLODSettings Settings;
	OverrunCrowd::SelectHighAgents(Candidates, PlayerLocations.Num(), Now, Settings);

	for (const OverrunCrowd::FLODCandidate& Candidate : Candidates)
	{
		if (Candidate.bWantHigh)
		{
			if (!Candidate.bHadHighTag)
			{
				Context.Defer().AddTag<FMassHighLODTag>(Candidate.Entity);
			}
			if (Candidate.bHadLowTag)
			{
				Context.Defer().RemoveTag<FMassLowLODTag>(Candidate.Entity);
			}
		}
		else
		{
			if (!Candidate.bHadLowTag)
			{
				Context.Defer().AddTag<FMassLowLODTag>(Candidate.Entity);
			}
			if (Candidate.bHadHighTag)
			{
				Context.Defer().RemoveTag<FMassHighLODTag>(Candidate.Entity);
			}
		}
		if (Candidate.bHadMediumTag)
		{
			Context.Defer().RemoveTag<FMassMediumLODTag>(Candidate.Entity);
		}
		if (Candidate.bHadOffTag)
		{
			Context.Defer().RemoveTag<FMassOffLODTag>(Candidate.Entity);
		}
	}
}
