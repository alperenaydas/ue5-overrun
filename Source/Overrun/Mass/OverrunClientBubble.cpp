#include "OverrunClientBubble.h"

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassReplicationSubsystem.h"
#include "OverrunEnemyHealthFragment.h"

FOverrunClientBubbleHandler::FOverrunClientBubbleHandler()
	: TransformHandler(*this)
{
}

void FOverrunClientBubbleHandler::Reset()
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	MovementCursor = 0;
#endif
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	MovementBindings.Reset();
#endif
	TClientBubbleHandlerBase<FOverrunAgentFastArrayItem>::Reset();
}

#if UE_REPLICATION_COMPILE_SERVER_CODE
void FOverrunClientBubbleHandler::QueueMovement(
	FMassReplicatedAgentHandle Handle, const FTransform& Transform, double SampleTime)
{
	check(IsInGameThread());
	check(AgentHandleManager.IsValidHandle(Handle));

	const int32 Index = AgentLookupArray[Handle.GetIndex()].AgentsIdx;
	FOverrunAgentFastArrayItem& Item = (*Agents)[Index];
	uint64 Pose = 0;
	if (!ensureMsgf(OverrunMovement::TryPackPose(Transform, Pose),
		TEXT("Overrun movement pose exceeds its coordinate contract")))
	{
		Item.bHasMovement = false;
		return;
	}

	Item.MovementPose = Pose;
	Item.MovementSampleTimeMs = uint32(uint64(SampleTime * 1000.0));
	Item.bHasMovement = true;
}

bool FOverrunClientBubbleHandler::BuildMovementPacket(
	double Now, int32 MaxRecords, FOverrunMovementPacket& OutPacket)
{
	check(IsInGameThread());
	OutPacket.Records.Reset();
	MaxRecords = FMath::Clamp(MaxRecords, 0, OverrunMovement::MaxRecords);
	const int32 Count = Agents->Num();
	if (Count == 0 || MaxRecords == 0)
	{
		return false;
	}

	OutPacket.Records.Reserve(MaxRecords);
	for (int32 Visited = 0; Visited < Count && OutPacket.Records.Num() < MaxRecords; ++Visited)
	{
		if (MovementCursor >= Count)
		{
			MovementCursor = 0;
		}

		FOverrunAgentFastArrayItem& Item = (*Agents)[MovementCursor++];
		if (!Item.bHasMovement)
		{
			continue;
		}

		check(Item.ReplicationID > 0);
		if (!Item.bMovementScheduleInitialized)
		{
			const double Phase = double(uint32(Item.ReplicationID) % 16u) / 16.0;
			Item.NextMovementTime = Now + Phase * OverrunMovement::Interval;
			Item.bMovementScheduleInitialized = true;
		}

		if (Now < Item.NextMovementTime)
		{
			continue;
		}

		FOverrunMovementRecord& Record = OutPacket.Records.AddDefaulted_GetRef();
		Record.Token = uint32(Item.ReplicationID);
		Record.SampleTimeMs = Item.MovementSampleTimeMs;
		Record.Pose = Item.MovementPose;

		Item.NextMovementTime += OverrunMovement::Interval;
		if (Item.NextMovementTime <= Now)
		{
			// Discard missed deadlines after a hitch instead of queuing old poses.
			Item.NextMovementTime = Now + OverrunMovement::Interval;
		}
	}

	return !OutPacket.Records.IsEmpty();
}

void FOverrunClientBubbleHandler::SetHitSerial(FMassReplicatedAgentHandle Handle, uint32 HitSerial)
{
	check(AgentHandleManager.IsValidHandle(Handle));
	const int32 Index = AgentLookupArray[Handle.GetIndex()].AgentsIdx;
	FOverrunAgentFastArrayItem& Item = (*Agents)[Index];

	if (Item.Agent.HitSerial != HitSerial)
	{
		Item.Agent.HitSerial = HitSerial;
		Serializer->MarkItemDirty(Item);
	}
}
#endif

#if UE_REPLICATION_COMPILE_CLIENT_CODE

void FOverrunClientBubbleHandler::PreReplicatedRemove(
	const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (const int32 Index : RemovedIndices)
	{
		MovementBindings.Remove((*Agents)[Index].ReplicationID);
	}

	TClientBubbleHandlerBase<FOverrunAgentFastArrayItem>::PreReplicatedRemove(RemovedIndices, FinalSize);
}

void FOverrunClientBubbleHandler::ApplyMovement(const FOverrunMovementPacket& Packet)
{
	check(IsInGameThread());
	FMassEntityManager& EntityManager = Serializer->GetEntityManagerChecked();
	UMassReplicationSubsystem* Replication = Serializer->GetReplicationSubsystem();
	if (!Replication)
	{
		return;
	}

	for (const FOverrunMovementRecord& Record : Packet.Records)
	{
		if (Record.Token > uint32(MAX_int32))
		{
			continue;
		}

		const int32 Token = int32(Record.Token);
		FMovementBinding* Binding = MovementBindings.Find(Token);
		// Movement never spawns an entity, including when it arrives before lifecycle data.
		if (!Binding || (Binding->bHasSequence &&
			!OverrunMovement::IsNewerSequence(Packet.Sequence, Binding->LastSequence)))
		{
			continue;
		}

		const FMassReplicationEntityInfo* Info = Replication->FindMassEntityInfo(Binding->NetID);
		if (!Info || Info->ReplicationID != Token || !Info->Entity.IsSet() ||
			!EntityManager.IsEntityValid(Info->Entity))
		{
			continue;
		}

		FVector Position;
		FQuat Rotation;
		OverrunMovement::UnpackPose(Record.Pose, Position, Rotation);
		FMassEntityView EntityView(EntityManager, Info->Entity);
		FTransform& Transform = EntityView.GetFragmentData<FTransformFragment>().GetMutableTransform();
		Transform.SetLocation(Position);
		Transform.SetRotation(Rotation);

		Binding->LastSequence = Packet.Sequence;
		Binding->LastSampleTimeMs = Record.SampleTimeMs;
		Binding->bHasSequence = true;
	}
}

void FOverrunClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		FTransformHandler::AddRequirementsForSpawnQuery(InQuery);
		InQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadWrite);
	};
	
	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& Context)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(Context);
	};
	
	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& ReplicatedEntity, const int32 EntityIndex)
	{
		TransformHandler.SetSpawnedEntityData(EntityIndex, ReplicatedEntity.GetReplicatedPositionYawData());
		EntityView.GetFragmentData<FOverrunEnemyHealthFragment>().HitSerial = ReplicatedEntity.HitSerial;
	};
	
	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& Item)
	{
		FTransformHandler::SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
		EntityView.GetFragmentData<FOverrunEnemyHealthFragment>().HitSerial = Item.HitSerial;
	};
	
	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();

	UMassReplicationSubsystem* Replication = Serializer->GetReplicationSubsystem();
	for (const int32 Index : AddedIndices)
	{
		const FOverrunAgentFastArrayItem& Item = (*Agents)[Index];
		const FMassReplicationEntityInfo* Info = Replication->FindMassEntityInfo(Item.Agent.GetNetID());
		if (Info && Info->ReplicationID == Item.ReplicationID && Info->Entity.IsSet())
		{
			// Preserve ordering state if the same add callback is replayed.
			MovementBindings.FindOrAdd(Item.ReplicationID).NetID = Item.Agent.GetNetID();
		}
	}
}

void FOverrunClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	PostReplicatedChangeHelper(ChangedIndices, [](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& Agent)
	{
		// The lifecycle pose is immutable: a hit update must not restore the spawn pose.
		EntityView.GetFragmentData<FOverrunEnemyHealthFragment>().HitSerial = Agent.HitSerial;
	});
}
#endif
