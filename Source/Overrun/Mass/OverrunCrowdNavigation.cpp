#include "OverrunCrowdNavigation.h"

#include "Avoidance/MassAvoidanceFragments.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "OverrunCrowdArrivalTrait.h"
#include "OverrunMassProbeTag.h"
#include "Steering/MassSteeringFragments.h"

void UOverrunCrowdNavigationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (World.IsNetMode(NM_Client) && !BuildContext.IsInspectingData())
	{
		return;
	}
	BuildContext.RequireTag<FOverrunMassProbeTag>();
	BuildContext.RequireTag<FMassCodeDrivenMovementTag>();
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.RequireFragment<FMassVelocityFragment>();
	BuildContext.RequireFragment<FMassDesiredMovementFragment>();
	BuildContext.RequireFragment<FMassForceFragment>();
	BuildContext.RequireFragment<FMassMoveTargetFragment>();
	BuildContext.RequireFragment<FMassGhostLocationFragment>();
	BuildContext.RequireFragment<FMassStandingSteeringFragment>();
	BuildContext.RequireFragment<FOverrunCrowdArrivalFragment>();
	BuildContext.RequireFragment<FMassNavigationEdgesFragment>();
	BuildContext.AddFragment<FOverrunCrowdNavigationFragment>();
	BuildContext.AddFragment<FMassNavMeshCachedPathFragment>();
	BuildContext.AddFragment<FMassNavMeshShortPathFragment>();
	BuildContext.AddFragment<FMassNavMeshBoundaryFragment>();
	FOverrunCrowdNavigationParameters Validated = Parameters;
	Validated.AgentHeight = FMath::Max(1.f, Validated.AgentHeight);
	Validated.ProjectionHeightTolerance = FMath::Clamp(Validated.ProjectionHeightTolerance, 1.f, 100.f);
	Validated.GoalMovementThreshold = FMath::Max(25.f, Validated.GoalMovementThreshold);
	Validated.RefreshInterval = FMath::Max(0.25f, Validated.RefreshInterval);
	Validated.RetryInterval = FMath::Max(0.25f, Validated.RetryInterval);
	Validated.NoProgressTimeout = FMath::Max(1.f, Validated.NoProgressTimeout);
	Validated.CorridorWidth = FMath::Max(100.f, Validated.CorridorWidth);
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(Validated));
}
