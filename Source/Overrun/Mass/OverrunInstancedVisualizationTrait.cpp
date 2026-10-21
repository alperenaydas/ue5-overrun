#include "OverrunInstancedVisualizationTrait.h"

#include "AnimToTextureDataAsset.h"
#include "Engine/World.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "OverrunEnemyPresentationFragment.h"

UOverrunInstancedVisualizationTrait::UOverrunInstancedVisualizationTrait()
{
	for (int32 LOD = 0; LOD < EMassLOD::Max; ++LOD)
	{
		Params.LODRepresentation[LOD] = EMassRepresentationType::StaticMeshInstance;
		LODParams.LODMaxCount[LOD] = MAX_int32;
	}
	Params.bKeepLowResActors = false;
	Params.bKeepActorExtraFrame = false;
	bAllowServerSideVisualization = false;
}

void UOverrunInstancedVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (World.IsNetMode(NM_DedicatedServer) && !BuildContext.IsInspectingData())
	{
		return;
	}
	Super::BuildTemplate(BuildContext, World);
	BuildContext.RequireFragment<FOverrunEnemyPresentationFragment>();
	BuildContext.AddTag<FOverrunInstancedPresentationTag>();
	BuildContext.AddFragment<FOverrunInstanceAnimationFragment>();
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(Animation));
}

bool UOverrunInstancedVisualizationTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	const bool bValid = Super::ValidateTemplate(BuildContext, World, OutTraitRequirements);
	if (World.IsNetMode(NM_DedicatedServer) && !BuildContext.IsInspectingData())
	{
		return bValid;
	}
	for (int32 LOD = EMassLOD::High; LOD < EMassLOD::Max; ++LOD)
	{
		if (Params.LODRepresentation[LOD] != EMassRepresentationType::StaticMeshInstance)
		{
			UE_LOG(LogTemp, Error, TEXT("Overrun Instanced Visualization requires instances in every visual tier, including Off. This baseline preserves the represented population; hybrid skeletal/instance phase matching is not implemented."));
			return false;
		}
		bool bCovered = false;
		for (const FMassStaticMeshInstanceVisualizationMeshDesc& Mesh : StaticMeshInstanceDesc.Meshes)
		{
			bCovered |= Mesh.Mesh && Mesh.MinLODSignificance <= float(LOD) && Mesh.MaxLODSignificance >= float(LOD + 1);
		}
		if (!bCovered)
		{
			UE_LOG(LogTemp, Error, TEXT("Overrun instanced mesh significance ranges must cover every LOD interval from 0 through 4."));
			return false;
		}
	}
	if (Animation.ReferenceMoveSpeed < 1.f || Animation.HitFlashDuration < 0.01f)
	{
		return false;
	}
	if (const UAnimToTextureDataAsset* Data = Animation.AnimationData)
	{
		if (Data->Mode != EAnimToTextureMode::Vertex || Data->NumFrames <= 0 || Data->SampleRate <= 0.f
			|| !Data->Animations.IsValidIndex(Animation.IdleAnimation)
			|| !Data->Animations.IsValidIndex(Animation.MoveAnimation))
		{
			UE_LOG(LogTemp, Error, TEXT("Overrun instance animation requires a baked vertex-mode AnimToTexture asset with valid idle and move clips."));
			return false;
		}
		for (const int32 ClipIndex : { Animation.IdleAnimation, Animation.MoveAnimation })
		{
			const FAnimToTextureAnimInfo& Clip = Data->Animations[ClipIndex];
			if (Clip.StartFrame < 0 || Clip.EndFrame < Clip.StartFrame || Clip.EndFrame >= Data->NumFrames)
			{
				UE_LOG(LogTemp, Error, TEXT("Overrun instance animation clip range is outside its baked frame atlas."));
				return false;
			}
		}
		for (const FMassStaticMeshInstanceVisualizationMeshDesc& Mesh : StaticMeshInstanceDesc.Meshes)
		{
			if (Mesh.Mesh && FSoftObjectPath(Mesh.Mesh) != Data->StaticMesh.ToSoftObjectPath())
			{
				UE_LOG(LogTemp, Error, TEXT("Overrun VAT instances must use the static mesh associated with their bake; vertex indices/UV lookup data are mesh-specific."));
				return false;
			}
		}
	}
	return bValid;
}
