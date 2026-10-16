// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunMassRepRemovalObserver.h"
#include "MassExecutionContext.h"
#include "MassReplicationSubsystem.h"
#include "MassReplicationFragments.h"
#include "OverrunClientBubbleInfo.h"
#include "OverrunMassProbeTag.h"

UOverrunMassRepRemovalObserver::UOverrunMassRepRemovalObserver() : EntityQuery(*this)
{
	ObservedType = FMassNetworkIDFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
	bRequiresGameThreadExecution = true;
}

void UOverrunMassRepRemovalObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSubsystemRequirement<UMassReplicationSubsystem>(EMassFragmentAccess::ReadWrite);
	Super::ConfigureQueries(EntityManager);
}

void UOverrunMassRepRemovalObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, ([this](FMassExecutionContext& Context)
		{
			const TConstArrayView<FMassNetworkIDFragment> NetworkIDs = Context.GetFragmentView<FMassNetworkIDFragment>();
			UMassReplicationSubsystem& ReplicationSubsystem = Context.GetMutableSubsystemChecked<UMassReplicationSubsystem>();
			const FMassBubbleInfoClassHandle BubbleClassHandle = ReplicationSubsystem.GetBubbleInfoClassHandle(AOverrunClientBubbleInfo::StaticClass());
			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle EntityHandle = Context.GetEntity(EntityIt);
				const FMassNetworkID NetID = NetworkIDs[EntityIt].NetID;
				for (const FMassClientHandle& ClientHandle : ReplicationSubsystem.GetClientReplicationHandles())
				{
					AOverrunClientBubbleInfo* BubbleInfo = ReplicationSubsystem.GetTypedClientBubble<AOverrunClientBubbleInfo>(BubbleClassHandle, ClientHandle);

					if (BubbleInfo)
					{
						BubbleInfo->GetAgentsSerializer().Bubble.RemoveAgent(NetID);
					}

					FMassClientReplicationInfo& ClientInfo = ReplicationSubsystem.GetMutableClientReplicationInfoChecked(ClientHandle);

					ClientInfo.AgentsData.Remove(EntityHandle);
					ClientInfo.HandledEntities.Remove(EntityHandle);
				}
			}
		}));
}
