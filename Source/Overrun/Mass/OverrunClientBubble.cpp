#include "OverrunClientBubble.h"

#include "OverrunEnemyHealthFragment.h"

FOverrunClientBubbleHandler::FOverrunClientBubbleHandler()
	: TransformHandler(*this)
{
}

#if UE_REPLICATION_COMPILE_SERVER_CODE
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
}

void FOverrunClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	PostReplicatedChangeHelper(ChangedIndices, [](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& Agent)
	{
		FTransformHandler::SetModifiedEntityData(EntityView, Agent.GetReplicatedPositionYawData());
		EntityView.GetFragmentData<FOverrunEnemyHealthFragment>().HitSerial = Agent.HitSerial;
	});
}
#endif
