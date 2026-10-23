#include "OverrunMassNavigationProcessor.h"

#include "Avoidance/MassAvoidanceFragments.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationBoundaryProcessor.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassSimulationLOD.h"
#include "Movement/MassMovementProcessors.h"
#include "NavCorridor.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshPath.h"
#include "OverrunCrowdArrival.h"
#include "OverrunCrowdNavigation.h"
#include "OverrunMassArrivalProcessor.h"
#include "OverrunMassMovementProcessor.h"
#include "OverrunMassProbeTag.h"
#include "Overrun/Character/TopDownCharacter.h"
#include "Overrun/Player/TopDownPlayerState.h"
#include "Steering/MassSteeringProcessors.h"

namespace
{
	constexpr int32 MaxRequestsPerFrame = 64;
	constexpr double RequestBudgetSeconds = 0.002;
	constexpr double NavPlaneTolerance = 10.0;

	FSharedConstNavQueryFilter GetFilter(const ARecastNavMesh& NavData, const FOverrunCrowdNavigationParameters& Params)
	{
		return Params.QueryFilter ? UNavigationQueryFilter::GetQueryFilter(NavData, Params.QueryFilter) : NavData.GetDefaultQueryFilter();
	}

	bool ProjectExact(const ARecastNavMesh& NavData, const FVector& Position,
		const FOverrunCrowdNavigationParameters& Params, FNavLocation& Result)
	{
		return !Position.ContainsNaN() && NavData.ProjectPoint(Position, Result,
			FVector(1.0, 1.0, Params.ProjectionHeightTolerance), GetFilter(NavData, Params)) && Result.NodeRef != INVALID_NAVNODEREF
			&& FVector::DistSquared2D(Position, Result.Location) <= FMath::Square(0.1)
			&& FMath::Abs(Position.Z - Result.Location.Z) <= Params.ProjectionHeightTolerance;
	}

	bool ClearSegment(const ARecastNavMesh& NavData, const FNavLocation& Start, const FVector& End,
		const FOverrunCrowdNavigationParameters& Params, FVector& Hit)
	{
		FNavigationRaycastAdditionalResults Results;
		const bool bHit = ARecastNavMesh::NavMeshRaycast(&NavData, Start.NodeRef, Start.Location, End,
			Hit, &Results, GetFilter(NavData, Params));
		return !bHit && Results.bIsRayEndInCorridor;
	}

	bool CompatibleNavData(const ARecastNavMesh* NavData, const float Radius, const float Height)
	{
		return NavData && NavData->GetConfig().AgentRadius + 0.1f >= Radius
			&& NavData->GetConfig().AgentHeight + 0.1f >= Height
			&& NavData->AgentRadius + 0.1f >= Radius && NavData->AgentHeight + 0.1f >= Height;
	}

	bool BlockingNormal(const ARecastNavMesh& NavData, const FNavLocation& Start, const FVector& End,
		const FOverrunCrowdNavigationParameters& Params, FVector& Normal)
	{
		ARecastNavMesh::FRaycastResult Result;
		FVector Hit;
		const bool bHit = ARecastNavMesh::NavMeshRaycast(&NavData, Start.Location, End, Hit,
			GetFilter(NavData, Params), nullptr, Result);
		if (!bHit || Result.CorridorPolysCount == 0 || Result.CorridorPolys[0] != Start.NodeRef) { return false; }
		Normal = FVector(Result.HitNormal.X, Result.HitNormal.Y, 0.0).GetSafeNormal();
		return !Normal.IsNearlyZero();
	}

	void SetAction(FMassMoveTargetFragment& Target, EMassMovementAction Action, const UWorld& World)
	{
		if (Target.GetCurrentAction() != Action || Target.GetCurrentActionID() == 0)
		{
			Target.CreateNewAction(Action, World);
		}
	}

	void ResetRoute(FOverrunCrowdNavigationFragment& State, FMassNavMeshCachedPathFragment& Cached,
		FMassNavMeshShortPathFragment& Short, FMassNavMeshBoundaryFragment& Boundary, FMassNavigationEdgesFragment& Edges)
	{
		Cached = FMassNavMeshCachedPathFragment();
		Short.Reset();
		Boundary.LastUpdatePosition = FVector(UE_BIG_NUMBER);
		Edges.AvoidanceEdges.Reset();
		State.NavData.Reset();
		State.bHasRoute = false;
		State.bDirectPath = false;
		State.DirectStartNode = INVALID_NAVNODEREF;
		State.WallSourceNode = INVALID_NAVNODEREF;
		State.bPartial = false;
		State.bFinalSegment = false;
		State.bCanMove = false;
	}

	void RefillShortPath(FMassNavMeshCachedPathFragment& Cached, FMassNavMeshShortPathFragment& Short,
		FMassNavMeshBoundaryFragment& Boundary, const bool bFirst)
	{
		const int32 Start = bFirst ? 0 : Cached.NavPathNextStartIndex;
		if (!Cached.Corridor || Start + 1 >= Cached.Corridor->Portals.Num())
		{
			Short.Reset();
			return;
		}
		Short.RequestShortPath(Cached.Corridor, Start, bFirst ? 0 : Cached.NumLeadingPoints, 20.f);
		Cached.NavPathNextStartIndex = uint16(Start + FMath::Max(int32(Short.NumPoints)
			- Short.NumPointsBeyondUpdate - Cached.NumLeadingPoints, 0));
		Boundary.LastUpdatePosition = FVector(UE_BIG_NUMBER);
	}

	bool InsideShortCorridor(const FVector& Position, const FMassNavMeshShortPathFragment& Short, const int32 FirstIndex = 0)
	{
		for (int32 Index = FirstIndex; Index + 1 < Short.NumPoints; ++Index)
		{
			const FVector Corners[] = {Short.Points[Index].Left, Short.Points[Index].Right,
				Short.Points[Index + 1].Right, Short.Points[Index + 1].Left};
			bool bPositive = false;
			bool bNegative = false;
			double Area = 0.0;
			for (int32 Edge = 0; Edge < 4; ++Edge)
			{
				const FVector A = Corners[(Edge + 1) % 4] - Corners[Edge];
				const FVector B = Position - Corners[Edge];
				const double Cross = A.X * B.Y - A.Y * B.X;
				Area += Corners[Edge].X * Corners[(Edge + 1) % 4].Y - Corners[(Edge + 1) % 4].X * Corners[Edge].Y;
				bPositive |= Cross > 0.1;
				bNegative |= Cross < -0.1;
			}
			if (FMath::Abs(Area) > 0.1 && !(bPositive && bNegative)) { return true; }
		}
		return false;
	}

	float PhysicalPathProgress(const FVector& Position, const FMassNavMeshShortPathFragment& Short)
	{
		if (Short.NumPoints < 2) { return 0.f; }
		double ClosestDistance = TNumericLimits<double>::Max();
		float Progress = 0.f;
		const int32 EndIndex = Short.bPartialResult ? Short.NumPoints - Short.NumPointsBeyondUpdate : Short.NumPoints - 1;
		for (int32 Index = 0; Index < EndIndex; ++Index)
		{
			const FVector A = Short.Points[Index].Position;
			const FVector Segment = Short.Points[Index + 1].Position - A;
			const FVector Delta = Position - A;
			const double Alpha = FMath::Clamp((Delta.X * Segment.X + Delta.Y * Segment.Y)
				/ FMath::Max(Segment.SizeSquared2D(), UE_DOUBLE_SMALL_NUMBER), 0.0, 1.0);
			const double Distance = FVector::DistSquared2D(Position, A + Segment * Alpha);
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				Progress = FMath::Lerp(Short.Points[Index].Distance.Get(), Short.Points[Index + 1].Distance.Get(), float(Alpha));
			}
		}
		return Progress;
	}
}

UOverrunMassNavigationProcessor::UOverrunMassNavigationProcessor() : NavigationQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = int32(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UOverrunMassMovementProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UMassNavMeshNavigationBoundaryProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UMassSteerToMoveTargetProcessor::StaticClass()->GetFName());
}

void UOverrunMassNavigationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery_Conditional.AddRequirement<FOverrunCrowdNavigationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddTagRequirement<FMassCodeDrivenMovementTag>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	NavigationQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	NavigationQuery.AddTagRequirement<FMassCodeDrivenMovementTag>(EMassFragmentPresence::All);
	NavigationQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	NavigationQuery.AddRequirement<FOverrunCrowdNavigationFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FMassNavMeshBoundaryFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FMassNavigationEdgesFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	NavigationQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	NavigationQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddRequirement<FOverrunCrowdArrivalFragment>(EMassFragmentAccess::ReadWrite);
	NavigationQuery.AddConstSharedRequirement<FOverrunCrowdNavigationParameters>();
	NavigationQuery.AddConstSharedRequirement<FOverrunCrowdArrivalParameters>();
	NavigationQuery.AddConstSharedRequirement<FMassMovementParameters>();
}

void UOverrunMassNavigationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	TArray<ATopDownCharacter*, TInlineAllocator<4>> Players;
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		ATopDownCharacter* Character = IsValid(Controller) ? Controller->GetPawn<ATopDownCharacter>() : nullptr;
		if (IsValid(Character) && IsValid(Character->GetPlayerState<ATopDownPlayerState>()) && !Character->IsDead()
			&& !Character->GetActorLocation().ContainsNaN())
		{
			Players.Add(Character);
		}
	}
	struct FCandidate
	{
		FMassEntityHandle Entity;
		double Since;
	};
	TArray<FCandidate> Candidates;
	NavigationQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const auto States = Chunk.GetMutableFragmentView<FOverrunCrowdNavigationFragment>();
		const auto Cached = Chunk.GetMutableFragmentView<FMassNavMeshCachedPathFragment>();
		const auto Shorts = Chunk.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
		const auto Boundaries = Chunk.GetMutableFragmentView<FMassNavMeshBoundaryFragment>();
		const auto Edges = Chunk.GetMutableFragmentView<FMassNavigationEdgesFragment>();
		const auto Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const auto Radii = Chunk.GetFragmentView<FAgentRadiusFragment>();
		const auto Arrivals = Chunk.GetFragmentView<FOverrunCrowdArrivalFragment>();
		const auto& Params = Chunk.GetConstSharedFragment<FOverrunCrowdNavigationParameters>();
		const auto& ArrivalParams = Chunk.GetConstSharedFragment<FOverrunCrowdArrivalParameters>();
		struct FProjectedGoal
		{
			const ARecastNavMesh* NavData = nullptr;
			const ATopDownCharacter* Player = nullptr;
			FNavLocation Location;
			bool bValid = false;
		};
		TArray<FProjectedGoal, TInlineAllocator<4>> ProjectedGoals;
		for (auto It = Chunk.CreateEntityIterator(); It; ++It)
		{
			auto& State = States[It];
			const bool bWasDirect = State.bDirectPath;
			const FMassEntityHandle Entity = Chunk.GetEntity(It);
			const FVector Position = Transforms[It].GetTransform().GetLocation();
			ATopDownCharacter* Closest = nullptr;
			double Distance = TNumericLimits<double>::Max();
			for (ATopDownCharacter* Player : Players)
			{
				const double TestDistance = FVector::DistSquared2D(Position, Player->GetActorLocation());
				if (TestDistance < Distance)
				{
					Distance = TestDistance;
					Closest = Player;
				}
			}
			const bool bDiscontinuity = !State.bInitialized || State.Identity != Entity
				|| FVector::DistSquared2D(Position, State.ExpectedPosition) > FMath::Square(1.0)
				|| FMath::Abs(Position.Z - State.ExpectedPosition.Z) > Params.ProjectionHeightTolerance;
			const bool bTargetChanged = State.Target.Get() != Closest || (!Closest && (State.bHasRoute || State.bDirectPath)) || (Closest && State.bInitialized
				&& FVector::DistSquared(State.Goal, Closest->GetNavAgentLocation()) > FMath::Square(1000.0));
			const bool bInvalidRoute = State.bHasRoute && (!State.NavData.IsValid() || !Cached[It].NavPath.IsValid()
				|| !Cached[It].NavPath->IsReady() || Cached[It].NavPath->GetPathPoints().IsEmpty() || !Cached[It].NavPath->IsUpToDate());
			if (bDiscontinuity || bTargetChanged || bInvalidRoute)
			{
				ResetRoute(State, Cached[It], Shorts[It], Boundaries[It], Edges[It]);
				State.bQueued = false;
				State.bRecoveryRepath = false;
				State.NextRecoveryTime = Now;
				State.NextRequestTime = Now + double(GetTypeHash(Entity) & 1023u) / 1024.0 * 0.25;
				State.ProgressPosition = Position;
				State.ProgressTime = Now;
			}
			State.Identity = Entity;
			State.bInitialized = true;
			State.ExpectedPosition = Position;
			State.Target = Closest;
			State.Goal = Closest ? Closest->GetNavAgentLocation() : Position;
			if (!Closest)
			{
				State.bQueued = false;
				State.bRecoveryRepath = false;
				State.bCanMove = false;
				continue;
			}
			const bool bMadeProgress = FVector::DistSquared2D(Position, State.ProgressPosition) > FMath::Square(25.0);
			if (bMadeProgress || !State.bCanMove || Arrivals[It].bHolding)
			{
				State.ProgressPosition = Position;
				State.ProgressTime = Now;
				if (bMadeProgress || Arrivals[It].bHolding) { State.bRecoveryRepath = false; }
			}
			const FVector NavPosition = Position - FVector(0, 0, Params.EntityHeightOffset);
			const ARecastNavMesh* NavData = State.NavData.Get();
			if (!CompatibleNavData(NavData, Radii[It].Radius, Params.AgentHeight))
			{
				const FNavAgentProperties Agent(Radii[It].Radius, Params.AgentHeight);
				NavData = Navigation ? Cast<ARecastNavMesh>(Navigation->GetNavDataForProps(Agent, NavPosition)) : nullptr;
			}
			if (!CompatibleNavData(NavData, Radii[It].Radius, Params.AgentHeight)) { NavData = nullptr; }
			FProjectedGoal* ProjectedGoal = nullptr;
			if (NavData)
			{
				ProjectedGoal = ProjectedGoals.FindByPredicate([&](const FProjectedGoal& Test)
				{
					return Test.NavData == NavData && Test.Player == Closest;
				});
				if (!ProjectedGoal)
				{
					ProjectedGoal = &ProjectedGoals.AddDefaulted_GetRef();
					ProjectedGoal->NavData = NavData;
					ProjectedGoal->Player = Closest;
					ProjectedGoal->bValid = ProjectExact(*NavData, State.Goal, Params, ProjectedGoal->Location);
				}
			}
			FNavLocation DirectStartLocation;
			FVector DirectHit;
			State.bDirectPath = NavData && ProjectedGoal && ProjectedGoal->bValid && ProjectExact(*NavData, NavPosition, Params, DirectStartLocation)
				&& FMath::Abs(DirectStartLocation.Location.Z - ProjectedGoal->Location.Location.Z) <= NavPlaneTolerance
				&& ClearSegment(*NavData, DirectStartLocation, ProjectedGoal->Location.Location, Params, DirectHit);
			const bool bWillHold = State.bDirectPath && ArrivalParams.ShouldHold(Arrivals[It].bHolding, true,
				FVector::Dist2D(Position, State.Goal));
			if (bWillHold) { State.bRecoveryRepath = false; }
			const bool bStuck = State.bCanMove && !Arrivals[It].bHolding && !bWillHold
				&& Now - State.ProgressTime >= Params.NoProgressTimeout && Now >= State.NextRecoveryTime;
			if (bStuck && !State.bRecoveryRepath)
			{
				State.bRecoveryRepath = true;
				State.NextRequestTime = FMath::Min(State.NextRequestTime, Now);
			}
			if (State.bDirectPath)
			{
				State.NavData = const_cast<ARecastNavMesh*>(NavData);
				State.NavPlaneZ = DirectStartLocation.Location.Z;
				State.DirectStartNode = DirectStartLocation.NodeRef;
				if (!bWasDirect) { State.WallSourceNode = INVALID_NAVNODEREF; }
				if (!State.bRecoveryRepath)
				{
					State.bQueued = false;
					State.NextRequestTime = Now;
				}
				if (!State.bRecoveryRepath) { continue; }
			}
			if (bWasDirect && !State.bDirectPath)
			{
				ResetRoute(State, Cached[It], Shorts[It], Boundaries[It], Edges[It]);
				State.NavData = const_cast<ARecastNavMesh*>(NavData);
				State.bQueued = false;
				State.NextRequestTime = Now;
			}
			const bool bGoalMoved = FVector::DistSquared(State.Goal, State.RequestedGoal) > FMath::Square(Params.GoalMovementThreshold);
			const bool bOutsideCorridor = State.bHasRoute && !State.bFinalSegment && Shorts[It].NumPoints >= 2
				&& !InsideShortCorridor(Position, Shorts[It]);
			const bool bRouteExhausted = State.bHasRoute && !State.bFinalSegment
				&& Shorts[It].IsDone() && !Shorts[It].bPartialResult;
			if (!State.bQueued && Now >= State.NextRequestTime && (!State.bHasRoute || bGoalMoved || State.bRecoveryRepath
				|| bOutsideCorridor || bRouteExhausted || Now >= State.NextRefreshTime))
			{
				State.bQueued = true;
				State.QueuedSince = Now;
			}
			if (State.bQueued)
			{
				Candidates.Add({Entity, State.QueuedSince});
			}
		}
	});
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.Since != B.Since) { return A.Since < B.Since; }
		if (A.Entity.Index != B.Entity.Index) { return A.Entity.Index < B.Entity.Index; }
		return A.Entity.SerialNumber < B.Entity.SerialNumber;
	});
	int32 Requests = 0;
	double RequestSeconds = 0.0;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Requests >= MaxRequestsPerFrame || RequestSeconds >= RequestBudgetSeconds) { break; }
		if (!EntityManager.IsEntityValid(Candidate.Entity)) { continue; }
		auto& State = EntityManager.GetFragmentDataChecked<FOverrunCrowdNavigationFragment>(Candidate.Entity);
		if (!State.bQueued || State.Identity != Candidate.Entity || !State.Target.IsValid()) { continue; }
		auto& Cached = EntityManager.GetFragmentDataChecked<FMassNavMeshCachedPathFragment>(Candidate.Entity);
		auto& Shorts = EntityManager.GetFragmentDataChecked<FMassNavMeshShortPathFragment>(Candidate.Entity);
		auto& Boundaries = EntityManager.GetFragmentDataChecked<FMassNavMeshBoundaryFragment>(Candidate.Entity);
		auto& Edges = EntityManager.GetFragmentDataChecked<FMassNavigationEdgesFragment>(Candidate.Entity);
		const auto& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(Candidate.Entity);
		const auto& Radii = EntityManager.GetFragmentDataChecked<FAgentRadiusFragment>(Candidate.Entity);
		const auto& Params = EntityManager.GetConstSharedFragmentDataChecked<FOverrunCrowdNavigationParameters>(Candidate.Entity);
		const auto& ArrivalParams = EntityManager.GetConstSharedFragmentDataChecked<FOverrunCrowdArrivalParameters>(Candidate.Entity);
		const bool bKeepDirectArrival = State.bDirectPath
			&& FVector::Dist2D(Transform.GetTransform().GetLocation(), State.Goal) <= ArrivalParams.CloseDistance;
		const double RequestStart = FPlatformTime::Seconds();
		++Requests;
		if (State.bRecoveryRepath)
		{
			State.bRecoveryRepath = false;
			State.ProgressTime = Now;
			State.ProgressPosition = Transform.GetTransform().GetLocation();
			State.NextRecoveryTime = Now + Params.NoProgressTimeout
				+ double(GetTypeHash(State.Identity) & 1023u) / 1024.0;
		}
		State.bQueued = false;
		State.RequestedGoal = State.Goal;
		State.NextRequestTime = Now + Params.RetryInterval;
		const FVector Position = Transform.GetTransform().GetLocation() - FVector(0, 0, Params.EntityHeightOffset);
		const FNavAgentProperties Agent(Radii.Radius, Params.AgentHeight);
		const ARecastNavMesh* NavData = Navigation ? Cast<ARecastNavMesh>(Navigation->GetNavDataForProps(Agent, Position)) : nullptr;
		const bool bCompatible = CompatibleNavData(NavData, Radii.Radius, Params.AgentHeight);
		FNavLocation Start;
		FNavLocation Goal;
		const bool bValidStart = bCompatible && ProjectExact(*NavData, Position, Params, Start);
		const bool bGoalOnNavMesh = bCompatible && ProjectExact(*NavData, State.Goal, Params, Goal);
		const bool bSamePlane = bValidStart && (bGoalOnNavMesh
			? FMath::Abs(Start.Location.Z - Goal.Location.Z) <= NavPlaneTolerance
			: FMath::Abs(Start.Location.Z - State.Goal.Z) <= Params.ProjectionHeightTolerance);
		if (bSamePlane)
		{
			const FVector QueryGoal = bGoalOnNavMesh ? Goal.Location : FVector(State.Goal.X, State.Goal.Y, Start.Location.Z);
			FPathFindingQuery Query(Navigation, *NavData, Start.Location, QueryGoal, GetFilter(*NavData, Params));
			Query.SetAllowPartialPaths(true);
			Query.SetRequireNavigableEndLocation(bGoalOnNavMesh);
			const FPathFindingResult Result = NavData->FindPath(Agent, Query);
			if (Result.IsSuccessful() && Result.Path.IsValid() && Result.Path->IsReady() && Result.Path->IsUpToDate())
			{
				Result.Path->EnableRecalculationOnInvalidation(false);
				const auto& PathPoints = Result.Path->GetPathPoints();
				const bool bFlatPath = !PathPoints.IsEmpty()
					&& FVector::DistSquared2D(PathPoints[0].Location, Start.Location) <= FMath::Square(0.1)
					&& !PathPoints.ContainsByPredicate([&](const FNavPathPoint& Point)
				{
					return FMath::Abs(Point.Location.Z - Start.Location.Z) > NavPlaneTolerance
						|| Point.CustomNavLinkId.IsValid() || FNavMeshNodeFlags(Point.Flags).IsNavLink();
				});
				Result.Path->RemoveOverlappingPoints(FNavCorridor::OverlappingPointTolerance);
				FNavLocation RouteEnd;
				const bool bValidEnd = bFlatPath && !PathPoints.IsEmpty() && ProjectExact(*NavData, PathPoints.Last().Location, Params, RouteEnd)
					&& FMath::Abs(Start.Location.Z - RouteEnd.Location.Z) <= NavPlaneTolerance;
				FVector EndpointHit;
				const bool bAtReachableEnd = bValidEnd && FVector::DistSquared2D(Start.Location, RouteEnd.Location) <= FMath::Square(20.0)
					&& ClearSegment(*NavData, Start, RouteEnd.Location, Params, EndpointHit);
				TSharedPtr<FNavCorridor> Corridor;
				if (bValidEnd && PathPoints.Num() > 1)
				{
					Corridor = MakeShared<FNavCorridor>();
					FNavCorridorParams CorridorParams;
					CorridorParams.SetFromWidth(Params.CorridorWidth);
					CorridorParams.PathOffsetFromBoundaries = 10.f;
					Corridor->BuildFromPath(*Result.Path, Query.QueryFilter, CorridorParams);
				}
				if (bValidEnd && ((Corridor && Corridor->IsValid() && Corridor->Portals.Num() < MAX_uint16) || bAtReachableEnd))
				{
					ResetRoute(State, Cached, Shorts, Boundaries, Edges);
					Cached.NavPath = Result.Path;
					Cached.Corridor = Corridor;
					Cached.PathSource = EMassNavigationPathSource::NavMesh;
					if (Corridor && Corridor->IsValid()) { RefillShortPath(Cached, Shorts, Boundaries, true); }
					State.NavData = const_cast<ARecastNavMesh*>(NavData);
					State.bHasRoute = true;
					State.bDirectPath = bKeepDirectArrival;
					State.DirectStartNode = bKeepDirectArrival ? Start.NodeRef : INVALID_NAVNODEREF;
					State.bPartial = Result.Path->IsPartial() || !bGoalOnNavMesh;
					State.NavPlaneZ = Start.Location.Z;
					State.NextRefreshTime = Now + (State.bPartial ? Params.RetryInterval : Params.RefreshInterval)
						* (1.0 + double(GetTypeHash(State.Identity) & 255u) / 1024.0);
					State.NextRequestTime = Now + (State.bPartial ? Params.RetryInterval : 0.25);
				}
			}
		}
		if (!bCompatible && !bWarnedMissingNavData)
		{
			UE_LOG(LogTemp, Warning, TEXT("Overrun navigation needs baked Recast data with agent radius/height at least as large as the crowd profile. Agents without a valid route wait; check the Navigation lab setup."));
			bWarnedMissingNavData = true;
		}
		RequestSeconds += FPlatformTime::Seconds() - RequestStart;
	}
	NavigationQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const auto States = Chunk.GetFragmentView<FOverrunCrowdNavigationFragment>();
		const auto Shorts = Chunk.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
		const auto Cached = Chunk.GetMutableFragmentView<FMassNavMeshCachedPathFragment>();
		const auto Boundaries = Chunk.GetMutableFragmentView<FMassNavMeshBoundaryFragment>();
		const auto Arrivals = Chunk.GetFragmentView<FOverrunCrowdArrivalFragment>();
		const auto Targets = Chunk.GetMutableFragmentView<FMassMoveTargetFragment>();
		const auto Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const auto& Params = Chunk.GetConstSharedFragment<FMassMovementParameters>();
		const auto& NavParams = Chunk.GetConstSharedFragment<FOverrunCrowdNavigationParameters>();
		for (auto It = Chunk.CreateEntityIterator(); It; ++It)
		{
			auto& Short = Shorts[It];
			const FVector Position = Transforms[It].GetTransform().GetLocation() - FVector(0, 0, NavParams.EntityHeightOffset);
			if (States[It].bHasRoute && !States[It].bDirectPath && Short.NumPoints >= 2)
			{
				float Progress = PhysicalPathProgress(Position, Short);
				const int32 UpdateIndex = Short.bPartialResult ? Short.NumPoints - Short.NumPointsBeyondUpdate : Short.NumPoints - 1;
				if (Short.bPartialResult && Progress >= Short.Points[UpdateIndex].Distance.Get() - Short.EndReachedDistance
					&& InsideShortCorridor(Position, Short, UpdateIndex - 1))
				{
					RefillShortPath(Cached[It], Short, Boundaries[It], false);
					Progress = PhysicalPathProgress(Position, Short);
				}
				if (Short.NumPoints >= 2)
				{
					const bool bReachedEnd = !Short.bPartialResult && FVector::DistSquared2D(Position,
						Short.Points[Short.NumPoints - 1].Position) <= FMath::Square(Short.EndReachedDistance);
					const float Cursor = FMath::Clamp(Short.MoveTargetProgressDistance, Progress, Progress + Short.EndReachedDistance);
					if (!FMath::IsNearlyEqual(Cursor, Short.MoveTargetProgressDistance) || bool(Short.bDone) != bReachedEnd)
					{
						Targets[It].bSteeringFallingBehind = false;
					}
					Short.MoveTargetProgressDistance = Cursor;
					Short.bDone = bReachedEnd;
				}
			}
			const bool bFollow = States[It].Target.IsValid() && (!States[It].bDirectPath || !Arrivals[It].bHolding) && (States[It].bDirectPath
				|| (States[It].bHasRoute && Shorts[It].NumPoints >= 2 && (!Shorts[It].IsDone() || Shorts[It].bPartialResult)));
			SetAction(Targets[It], bFollow ? EMassMovementAction::Move : EMassMovementAction::Stand, *World);
			Targets[It].DesiredSpeed.Set(bFollow ? FMath::Min(300.f, Params.MaxSpeed) : 0.f);
			Targets[It].SlackRadius = 0.f;
			Targets[It].IntentAtGoal = EMassMovementAction::Stand;
			if (!bFollow) { Targets[It].Center = Transforms[It].GetTransform().GetLocation(); }
		}
	});
	Super::Execute(EntityManager, Context);
	NavigationQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const auto States = Chunk.GetMutableFragmentView<FOverrunCrowdNavigationFragment>();
		const auto Shorts = Chunk.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
		const auto Boundaries = Chunk.GetMutableFragmentView<FMassNavMeshBoundaryFragment>();
		const auto Edges = Chunk.GetMutableFragmentView<FMassNavigationEdgesFragment>();
		const auto Targets = Chunk.GetMutableFragmentView<FMassMoveTargetFragment>();
		const auto Transforms = Chunk.GetMutableFragmentView<FTransformFragment>();
		const auto Velocities = Chunk.GetFragmentView<FMassVelocityFragment>();
		const auto Arrivals = Chunk.GetMutableFragmentView<FOverrunCrowdArrivalFragment>();
		const auto& Params = Chunk.GetConstSharedFragment<FOverrunCrowdNavigationParameters>();
		const auto& ArrivalParams = Chunk.GetConstSharedFragment<FOverrunCrowdArrivalParameters>();
		const auto& MovementParams = Chunk.GetConstSharedFragment<FMassMovementParameters>();
		TArray<FNavigationPortalEdge> Walls;
		Walls.Reserve(64);
		for (auto It = Chunk.CreateEntityIterator(); It; ++It)
		{
			auto& State = States[It];
			auto& Arrival = Arrivals[It];
			auto& Target = Targets[It];
			FTransform& Transform = Transforms[It].GetMutableTransform();
			const FVector Position = Transform.GetLocation();
			auto& Short = Shorts[It];
			if (!State.bDirectPath && State.bHasRoute && Short.NumPoints >= 2 && !Short.bPartialResult && Short.IsDone())
			{
				const FVector End = Short.Points[Short.NumPoints - 1].Position;
				const FVector NavPosition = Position - FVector(0, 0, Params.EntityHeightOffset);
				const double Remaining = FVector::Dist2D(NavPosition, End);
				if (Remaining > Short.EndReachedDistance)
				{
					Short.bDone = false;
					Target.Center = End;
					Target.Forward = FVector(End.X - Position.X, End.Y - Position.Y, 0).GetSafeNormal();
					Target.DistanceToGoal = float(Remaining);
					Target.EntityDistanceToGoal = FMassMoveTargetFragment::UnsetDistance;
					Target.bSteeringFallingBehind = false;
				}
			}
			const FVector ToPlayer = FVector(State.Goal.X - Position.X, State.Goal.Y - Position.Y, 0);
			Arrival.bHasTarget = State.Target.IsValid();
			Arrival.TargetLocation = FVector(State.Goal.X, State.Goal.Y, Position.Z);
			Arrival.DistanceToTarget = Arrival.bHasTarget ? ToPlayer.Size() : TNumericLimits<double>::Max();
			State.bFinalSegment = State.bDirectPath && Arrival.bHasTarget;
			const bool bWasHolding = Arrival.bHolding;
			Arrival.bHolding = State.bFinalSegment && ArrivalParams.ShouldHold(bWasHolding, Arrival.bHasTarget, Arrival.DistanceToTarget);
			if (Arrival.bHolding && !bWasHolding) { Arrival.HoldLocation = Position; }
			State.bCanMove = Arrival.bHasTarget && !Arrival.bHolding && (State.bDirectPath
				|| (State.bHasRoute && Shorts[It].NumPoints >= 2 && (!Shorts[It].IsDone() || Shorts[It].bPartialResult)));
			if (State.bFinalSegment)
			{
				const double Remaining = FMath::Max(0.0, Arrival.DistanceToTarget - ArrivalParams.StopDistance);
				Target.Center = Arrival.bHolding ? Arrival.HoldLocation : Position + ToPlayer.GetSafeNormal() * Remaining;
				if (!Arrival.bHolding) { Target.Center.Z = State.NavPlaneZ + Params.EntityHeightOffset; }
				Target.Forward = ToPlayer.GetSafeNormal();
				Target.DistanceToGoal = float(Remaining);
				Target.EntityDistanceToGoal = FMassMoveTargetFragment::UnsetDistance;
			}
			else if (State.bCanMove)
			{
				Target.Center.Z += Params.EntityHeightOffset;
			}
			SetAction(Target, State.bCanMove ? EMassMovementAction::Move : EMassMovementAction::Stand, *World);
			Target.DesiredSpeed.Set(State.bCanMove ? FMath::Min(300.f, MovementParams.MaxSpeed) : 0.f);
			if (!State.bCanMove)
			{
				Target.Center = Position;
				Target.DistanceToGoal = 0.f;
				Target.EntityDistanceToGoal = FMassMoveTargetFragment::UnsetDistance;
			}
			if (State.bDirectPath)
			{
				const ARecastNavMesh* NavData = State.NavData.Get();
				if (State.WallSourceNode != State.DirectStartNode)
				{
					Edges[It].AvoidanceEdges.Reset();
					Edges[It].bExtrudedEdges = true;
					Walls.Reset();
					if (NavData && NavData->GetPolyWallSegments(State.DirectStartNode, GetFilter(*NavData, Params), nullptr, Walls))
					{
						State.WallSourceNode = State.DirectStartNode;
						for (const FNavigationPortalEdge& Wall : Walls)
						{
							if (Wall.ToRef != INVALID_NAVNODEREF || Edges[It].AvoidanceEdges.Num() >= Edges[It].MaxEdgesCount) { continue; }
							FNavigationAvoidanceEdge Edge(Wall.Left, Wall.Right);
							if (FVector::DotProduct(Edge.LeftDir, Position - Wall.Left) < 0.0)
							{
								Edge = FNavigationAvoidanceEdge(Wall.Right, Wall.Left);
							}
							Edges[It].AvoidanceEdges.Add(Edge);
						}
					}
				}
				Boundaries[It].LastUpdatePosition = Target.Center;
			}
			FVector Facing = Arrival.bHolding ? ToPlayer : Velocities[It].Value;
			Facing.Z = 0;
			if (Facing.SizeSquared() < FMath::Square(5.0)) { Facing = ToPlayer; }
			if (!Facing.IsNearlyZero())
			{
				Transform.SetRotation(FMath::RInterpConstantTo(Transform.Rotator(), Facing.Rotation(), Chunk.GetDeltaTimeSeconds(), 360.f).Quaternion());
			}
		}
	});
}

UOverrunMassNavigationConstraintProcessor::UOverrunMassNavigationConstraintProcessor() : ConstraintQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = int32(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::ApplyForces);
	ExecutionOrder.ExecuteAfter.Add(UOverrunMassArrivalProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UMassApplyMovementProcessor::StaticClass()->GetFName());
}

void UOverrunMassNavigationConstraintProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ConstraintQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	ConstraintQuery.AddTagRequirement<FMassCodeDrivenMovementTag>(EMassFragmentPresence::All);
	ConstraintQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	ConstraintQuery.AddRequirement<FOverrunCrowdNavigationFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ConstraintQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddRequirement<FMassStandingSteeringFragment>(EMassFragmentAccess::ReadWrite);
	ConstraintQuery.AddConstSharedRequirement<FOverrunCrowdNavigationParameters>();
}

void UOverrunMassNavigationConstraintProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!Context.GetWorld() || Context.GetWorld()->GetNetMode() == NM_Client) { return; }
	const float DeltaTime = FMath::Clamp(Context.GetDeltaTimeSeconds(), 0.f, 0.1f);
	ConstraintQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const auto States = Chunk.GetMutableFragmentView<FOverrunCrowdNavigationFragment>();
		const auto Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const auto Velocities = Chunk.GetMutableFragmentView<FMassVelocityFragment>();
		const auto Movement = Chunk.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const auto Forces = Chunk.GetMutableFragmentView<FMassForceFragment>();
		const auto Targets = Chunk.GetMutableFragmentView<FMassMoveTargetFragment>();
		const auto Ghosts = Chunk.GetMutableFragmentView<FMassGhostLocationFragment>();
		const auto Standing = Chunk.GetMutableFragmentView<FMassStandingSteeringFragment>();
		const auto& Params = Chunk.GetConstSharedFragment<FOverrunCrowdNavigationParameters>();
		for (auto It = Chunk.CreateEntityIterator(); It; ++It)
		{
			auto& State = States[It];
			const FVector Position = Transforms[It].GetTransform().GetLocation();
			const FVector Desired = Movement[It].DesiredVelocity;
			FVector Accepted = FVector(Desired.X, Desired.Y, 0);
			const ARecastNavMesh* NavData = State.NavData.Get();
			FNavLocation Start;
			bool bBlocked = !State.bCanMove || !NavData || Desired.ContainsNaN() || DeltaTime <= UE_SMALL_NUMBER;
			if (!bBlocked)
			{
				bBlocked = !ProjectExact(*NavData, Position - FVector(0, 0, Params.EntityHeightOffset), Params, Start);
				bBlocked |= FMath::Abs(Start.Location.Z - State.NavPlaneZ) > NavPlaneTolerance;
				if (!bBlocked && !Accepted.IsNearlyZero())
				{
					const FVector Displacement = Accepted * DeltaTime;
					FVector Hit = Start.Location;
					if (!ClearSegment(*NavData, Start, Start.Location + Displacement, Params, Hit))
					{
						const double Length = Displacement.Size2D();
						const double Fraction = FMath::Clamp((FVector::Dist2D(Start.Location, Hit) - 2.0) / FMath::Max(Length, 1.0), 0.0, 1.0);
						Accepted *= Fraction;
						FVector Normal;
						if (BlockingNormal(*NavData, Start, Start.Location + Displacement, Params, Normal))
						{
							const FVector Prefix = Accepted * DeltaTime;
							const FVector Remaining = Displacement - Prefix;
							const FVector Slide = Prefix + Remaining - Normal * FVector::DotProduct(Remaining, Normal);
							FVector SlideHit;
							if (Slide.SizeSquared2D() > Prefix.SizeSquared2D() + UE_DOUBLE_SMALL_NUMBER
								&& ClearSegment(*NavData, Start, Start.Location + Slide, Params, SlideHit))
							{
								Accepted = Slide / DeltaTime;
							}
						}
						FVector Recheck;
						if (!ClearSegment(*NavData, Start, Start.Location + Accepted * DeltaTime, Params, Recheck))
						{
							Accepted = FVector::ZeroVector;
						}
					}
				}
			}
			if (bBlocked) { Accepted = FVector::ZeroVector; }
			if (!Accepted.Equals(Desired, 0.01))
			{
				const float DesiredSpeed = Targets[It].DesiredSpeed.Get();
				OverrunCrowd::HoldArrivalAtPosition(Position, Velocities[It], Movement[It], Forces[It], Targets[It], Ghosts[It], Standing[It]);
				if (State.bCanMove) { Targets[It].DesiredSpeed.Set(DesiredSpeed); }
				Ghosts[It].Location = Position + Accepted * DeltaTime;
				Ghosts[It].Velocity = Accepted;
			}
			Movement[It].DesiredVelocity = Accepted;
			Velocities[It].Value = Accepted;
			State.ExpectedPosition = Position + Accepted * DeltaTime;
		}
	});
}
