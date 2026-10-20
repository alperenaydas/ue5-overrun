#include "OverrunCrowdArrivalTrait.h"

#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "OverrunMassProbeTag.h"
#include "Steering/MassSteeringFragments.h"

void UOverrunCrowdArrivalTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
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
	BuildContext.AddFragment<FOverrunCrowdArrivalFragment>();
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(Parameters.GetValidated()));
}
