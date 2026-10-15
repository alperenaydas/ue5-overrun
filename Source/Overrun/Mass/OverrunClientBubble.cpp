#include "OverrunClientBubble.h"

FOverrunClientBubbleHandler::FOverrunClientBubbleHandler()
	: TransformHandler(*this)
{
}

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FOverrunClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		FTransformHandler::AddRequirementsForSpawnQuery(InQuery);
	};
	
	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& Context)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(Context);
	};
	
	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& ReplicatedEntity, const int32 EntityIndex)
	{
		TransformHandler.SetSpawnedEntityData(EntityIndex, ReplicatedEntity.GetReplicatedPositionYawData());
	};
	
	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& Item)
	{
		FTransformHandler::SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
	};
	
	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();
}

void FOverrunClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	PostReplicatedChangeHelper(ChangedIndices, [](const FMassEntityView& EntityView, const FOverrunReplicatedAgent& Agent)
	{
		FTransformHandler::SetModifiedEntityData(EntityView, Agent.GetReplicatedPositionYawData());
	});
}
#endif
