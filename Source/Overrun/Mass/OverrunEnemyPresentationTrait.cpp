#include "OverrunEnemyPresentationTrait.h"

#include "Engine/World.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassRepresentationFragments.h"
#include "OverrunEnemyHitPresentationFragment.h"
#include "OverrunEnemyPresentationFragment.h"
#include "OverrunMassProbeTag.h"
#include "Translators/MassCapsuleComponentTranslators.h"
#include "Translators/MassCharacterMovementTranslators.h"
#include "Translators/MassSceneComponentLocationTranslator.h"

void UOverrunEnemyPresentationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (World.IsNetMode(NM_DedicatedServer) && !BuildContext.IsInspectingData())
	{
		return;
	}

	BuildContext.RequireTag<FOverrunMassProbeTag>();
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();
	BuildContext.RequireFragment<FMassRepresentationFragment>();
	BuildContext.AddFragment<FOverrunEnemyPresentationFragment>();
	BuildContext.AddFragment<FOverrunEnemyHitPresentationFragment>();
}

bool UOverrunEnemyPresentationTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	const bool bValid = Super::ValidateTemplate(BuildContext, World, OutTraitRequirements);
	if (World.IsNetMode(NM_DedicatedServer) && !BuildContext.IsInspectingData())
	{
		return bValid;
	}

	if (BuildContext.HasTag<FMassCapsuleTransformCopyToActorTag>()
		|| BuildContext.HasTag<FMassCharacterMovementCopyToActorTag>()
		|| BuildContext.HasTag<FMassCharacterOrientationCopyToActorTag>()
		|| BuildContext.HasTag<FMassSceneComponentLocationCopyToActorTag>()
		|| BuildContext.HasTag<FMassCapsuleTransformCopyToMassTag>()
		|| BuildContext.HasTag<FMassCharacterMovementCopyToMassTag>()
		|| BuildContext.HasTag<FMassCharacterOrientationCopyToMassTag>()
		|| BuildContext.HasTag<FMassSceneComponentLocationCopyToMassTag>())
	{
		UE_LOG(LogTemp, Error, TEXT("Overrun Enemy Presentation requires visual-only Actors without Agent component sync traits. Remove native Actor/Mass transform and movement sync traits from this enemy configuration."));
		return false;
	}
	return bValid;
}
