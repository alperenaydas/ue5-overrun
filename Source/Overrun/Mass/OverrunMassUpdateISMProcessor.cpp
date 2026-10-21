#include "OverrunMassUpdateISMProcessor.h"

#include "AnimToTextureDataAsset.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"
#include "OverrunEnemyHealthFragment.h"
#include "OverrunEnemyPresentationFragment.h"
#include "OverrunInstancedVisualizationTrait.h"
#include "OverrunMassTransformProcessor.h"

namespace
{
	struct FInstanceData
	{
		float Frame = 0.f;
		float PreviousFrame = 0.f;
		float HitFlash = 0.f;
		float GroundSpeed = 0.f;
	};
}

UOverrunMassUpdateISMProcessor::UOverrunMassUpdateISMProcessor() : InstancedQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ExecutionOrder.ExecuteAfter.Add(UOverrunMassTransformProcessor::StaticClass()->GetFName());
}

void UOverrunMassUpdateISMProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery.AddTagRequirement<FOverrunInstancedPresentationTag>(EMassFragmentPresence::None);

	InstancedQuery.AddTagRequirement<FOverrunInstancedPresentationTag>(EMassFragmentPresence::All);
	InstancedQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::None);
	InstancedQuery.AddRequirement<FOverrunEnemyPresentationFragment>(EMassFragmentAccess::ReadOnly);
	InstancedQuery.AddRequirement<FOverrunInstanceAnimationFragment>(EMassFragmentAccess::ReadWrite);
	InstancedQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	InstancedQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	InstancedQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	InstancedQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadWrite);
	InstancedQuery.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
	InstancedQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	InstancedQuery.AddConstSharedRequirement<FOverrunInstanceAnimationParameters>();
}

void UOverrunMassUpdateISMProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!Context.GetWorld() || Context.GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OverrunCrowdInstances);
	Super::Execute(EntityManager, Context);
	InstancedQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* Subsystem = Context.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(Subsystem);
		FMassInstancedStaticMeshInfoArrayView Infos = Subsystem->GetMutableInstancedStaticMeshInfos();
		const auto Poses = Context.GetFragmentView<FOverrunEnemyPresentationFragment>();
		const auto LODs = Context.GetFragmentView<FMassRepresentationLODFragment>();
		const auto Healths = Context.GetFragmentView<FOverrunEnemyHealthFragment>();
		const auto Representations = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		const auto Animations = Context.GetMutableFragmentView<FOverrunInstanceAnimationFragment>();
		const auto& Parameters = Context.GetConstSharedFragment<FOverrunInstanceAnimationParameters>();
		const UAnimToTextureDataAsset* Data = Parameters.AnimationData;
		const float DeltaSeconds = FMath::Max(0.f, Context.GetDeltaTimeSeconds());

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FMassRepresentationFragment& Representation = Representations[It];
			FOverrunInstanceAnimationFragment& Animation = Animations[It];
			const FOverrunEnemyPresentationFragment& Pose = Poses[It];
			if (Representation.CurrentRepresentation != EMassRepresentationType::StaticMeshInstance || !Pose.bInitialized)
			{
				Animation.Reset();
				continue;
			}
			const int32 InfoIndex = Representation.StaticMeshDescHandle.ToIndex();
			if (!ensure(Infos.IsValidIndex(InfoIndex)))
			{
				continue;
			}

			const bool bFirstFrame = !Animation.bInitialized || Animation.LastUpdateFrame + 1 != GFrameCounter;
			const uint32 HitSerial = Healths.IsEmpty() ? 0 : Healths[It].HitSerial;
			Animation.HitFlashRemaining = FMath::Max(0.f, Animation.HitFlashRemaining - DeltaSeconds);
			if (bFirstFrame)
			{
				Animation.HitFlashRemaining = 0.f;
				Animation.Phase = float(GetTypeHash(Context.GetEntity(It)) & 65535u) / 65536.f;
			}
			else if (HitSerial != Animation.LastHitSerial)
			{
				Animation.HitFlashRemaining = Parameters.HitFlashDuration;
			}
			Animation.LastHitSerial = HitSerial;

			FInstanceData CustomData;
			CustomData.GroundSpeed = Pose.GroundSpeed;
			CustomData.HitFlash = Animation.HitFlashRemaining / Parameters.HitFlashDuration;
			const bool bWasMoving = Animation.bMoving;
			Animation.bMoving = Pose.GroundSpeed > (bWasMoving ? 3.f : 8.f);
			const int32 ClipIndex = Animation.bMoving ? Parameters.MoveAnimation : Parameters.IdleAnimation;
			if (Data && Data->Animations.IsValidIndex(ClipIndex))
			{
				const FAnimToTextureAnimInfo& Clip = Data->Animations[ClipIndex];
				const float FrameSpan = float(Clip.EndFrame - Clip.StartFrame);
				CustomData.Frame = float(Clip.StartFrame);
				if (FrameSpan > 0.f)
				{
					const float PlayRate = Animation.bMoving
						? FMath::Clamp(Pose.GroundSpeed / Parameters.ReferenceMoveSpeed, 0.5f, 1.5f) : 1.f;
					Animation.Phase = FMath::Frac(Animation.Phase + DeltaSeconds * PlayRate * Data->SampleRate / FrameSpan);
					CustomData.Frame = FMath::Min(float(Clip.EndFrame), float(Clip.StartFrame) + Animation.Phase * FrameSpan);
				}
			}
			CustomData.PreviousFrame = bFirstFrame || Pose.bResetThisFrame || bWasMoving != Animation.bMoving
				? CustomData.Frame : Animation.PreviousFrame;
			Animation.PreviousFrame = CustomData.Frame;
			Animation.bInitialized = true;
			Animation.LastUpdateFrame = GFrameCounter;

			const FTransform& Previous = bFirstFrame || Pose.bResetThisFrame ? Pose.DisplayTransform : Representation.PrevTransform;
			FMassInstancedStaticMeshInfo& Info = Infos[InfoIndex];
			UpdateISMTransform(Context.GetEntity(It), Info, Pose.DisplayTransform, Previous,
				LODs[It].LODSignificance, Representation.PrevLODSignificance);
			Info.AddBatchedCustomData(CustomData, LODs[It].LODSignificance, Representation.PrevLODSignificance);
			Representation.PrevTransform = Pose.DisplayTransform;
			Representation.PrevLODSignificance = LODs[It].LODSignificance;
		}
	});
}
