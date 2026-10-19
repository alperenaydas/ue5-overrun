// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunEnemyDamageSubsystem.h"

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "OverrunEnemyHealthFragment.h"
#include "OverrunEnemyHitShapeFragment.h"
#include "OverrunMassProbeTag.h"
#include "Chaos/Capsule.h"
#include "Engine/World.h"

void UOverrunEnemyDamageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassEntitySubsystem>();
	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	check(MassSubsystem);

	EnemyHitQuery.Initialize(MassSubsystem->GetMutableEntityManager().AsShared());
	EnemyHitQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EnemyHitQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyHitQuery.AddRequirement<FOverrunEnemyHitShapeFragment>(EMassFragmentAccess::ReadOnly);
	EnemyHitQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadOnly);
}

bool UOverrunEnemyDamageSubsystem::EnqueueDamage(const FOverrunEnemyDamageCommand& Command)
{
	if (!CanAccessQueue())
	{
		return false;
	}
	
	if (!Command.TargetEntity.IsSet()
		|| !FMath::IsFinite(Command.Damage)
		|| Command.Damage <= 0.f
		|| PendingCommands.Num() >= MaxPendingCommands)
	{
		return false;
	}

	PendingCommands.Add(Command);
	return true;
}

TArray<FOverrunEnemyDamageCommand> UOverrunEnemyDamageSubsystem::ConsumePendingDamage()
{
	if (!CanAccessQueue())
	{
		return {};
	}
	
	TArray<FOverrunEnemyDamageCommand> Commands = MoveTemp(PendingCommands);
	PendingCommands.Reset();
	return Commands;
}

bool UOverrunEnemyDamageSubsystem::FindClosestEnemyHit(const FVector& Start, const FVector& UnitDirection,
	float MaxDistance, FMassEntityHandle& OutEntity, float& OutDistance)
{
	OutEntity.Reset();
	OutDistance = 0.f;

	if (!CanAccessQueue())
	{
		return false;
	}

	if (Start.ContainsNaN() ||
		UnitDirection.ContainsNaN() ||
		!UnitDirection.IsNormalized() ||
		!FMath::IsFinite(MaxDistance) ||
		MaxDistance <= 0.f)
	{
		return false;
	}
	
	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();

	if (!MassSubsystem)
	{
		return false;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	FMassExecutionContext Context = EntityManager.CreateExecutionContext(0.f);
	bool bFoundHit = false;
	EnemyHitQuery.ForEachEntityChunk(Context, [&Start, &OutEntity, &OutDistance, &bFoundHit, &UnitDirection, MaxDistance](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FOverrunEnemyHealthFragment> Healths = ChunkContext.GetFragmentView<FOverrunEnemyHealthFragment>();
		const TConstArrayView<FTransformFragment> Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FOverrunEnemyHitShapeFragment> HitShapes = ChunkContext.GetFragmentView<FOverrunEnemyHitShapeFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			if (Healths[EntityIt].Health <= 0.f)
			{
				continue;
			}
			
			const FOverrunEnemyHitShapeFragment& Shape = HitShapes[EntityIt];
			const FVector Center = Transforms[EntityIt].GetTransform().GetLocation() + FVector(0.f, 0.f, Shape.CenterZOffset);

			const double SegmentHalfLength = Shape.HalfHeight - Shape.Radius;
			const FVector AxisOffset(0.0, 0.0, SegmentHalfLength);

			const FVector CapsuleBottom = Center - AxisOffset;
			const FVector CapsuleTop = Center + AxisOffset;
			const Chaos::FCapsule Capsule(CapsuleBottom, CapsuleTop, Shape.Radius);
			Chaos::FReal HitDistance = 0;
			Chaos::FVec3 HitPosition;
			Chaos::FVec3 HitNormal;
			int32 FaceIndex = INDEX_NONE;

			const bool bHit = Capsule.Raycast(Start, UnitDirection, MaxDistance, 0.0, HitDistance, HitPosition, HitNormal, FaceIndex);
			if (!bHit)
			{
				continue;
			}
			if (!bFoundHit || HitDistance < OutDistance)
			{
				OutEntity = ChunkContext.GetEntity(EntityIt);
				OutDistance = static_cast<float>(HitDistance);
				bFoundHit = true;
			}
		}
	});
	
	return bFoundHit;
}

bool UOverrunEnemyDamageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UOverrunEnemyDamageSubsystem::CanAccessQueue() const
{
	if (!IsInGameThread())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}