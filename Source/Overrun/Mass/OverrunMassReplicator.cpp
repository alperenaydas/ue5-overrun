// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Mass/OverrunMassReplicator.h"
#include "MassReplicationTransformHandlers.h"
#include "MassExecutionContext.h"
#include "OverrunClientBubbleInfo.h"
#include "OverrunEnemyHealthFragment.h"
#include "OverrunReplicatedAgent.h"

void UOverrunMassReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
	EntityQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadOnly);
}

void UOverrunMassReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	
	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;
	TConstArrayView<FOverrunEnemyHealthFragment> Healths;

	auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler, &Healths](FMassExecutionContext& Context)
	{
		PositionYawHandler.CacheFragmentViews(Context);
		RepSharedFrag = &Context.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		Healths = Context.GetFragmentView<FOverrunEnemyHealthFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler, &Healths](FMassExecutionContext& Context, const int32 EntityIdx, FOverrunReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle)->FMassReplicatedAgentHandle
	{
		AOverrunClientBubbleInfo& CrowdBubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AOverrunClientBubbleInfo>(ClientHandle);
		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());
		InReplicatedAgent.HitSerial = Healths[EntityIdx].HitSerial;
		return CrowdBubbleInfo.GetAgentsSerializer().Bubble.AddAgent(Context.GetEntity(EntityIdx), InReplicatedAgent);
	};
	
	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler, &Healths](FMassExecutionContext& Context, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AOverrunClientBubbleInfo& CrowdBubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AOverrunClientBubbleInfo>(ClientHandle);
		FOverrunClientBubbleHandler& Bubble = CrowdBubbleInfo.GetAgentsSerializer().Bubble;
		PositionYawHandler.ModifyEntity<FOverrunAgentFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
		Bubble.SetHitSerial(Handle, Healths[EntityIdx].HitSerial);
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& Context, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AOverrunClientBubbleInfo& CrowdBubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AOverrunClientBubbleInfo>(ClientHandle);
		CrowdBubbleInfo.GetAgentsSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FOverrunAgentFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif
}
