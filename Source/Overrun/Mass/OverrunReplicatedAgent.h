#pragma once
#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "OverrunReplicatedAgent.generated.h"

USTRUCT()
struct FOverrunReplicatedAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()
	
	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }
	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }
public:
	UPROPERTY()
	uint32 HitSerial = 0;
private:
	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;
};

USTRUCT()
struct FOverrunAgentFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FOverrunAgentFastArrayItem() = default;
	FOverrunAgentFastArrayItem(const FOverrunReplicatedAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}
	
	typedef FOverrunReplicatedAgent FReplicatedAgentType;

	UPROPERTY()
	FOverrunReplicatedAgent Agent;

#if UE_REPLICATION_COMPILE_SERVER_CODE
	uint64 MovementPose = 0;
	uint32 MovementSampleTimeMs = 0;
	double NextMovementTime = 0.0;
	bool bHasMovement = false;
	bool bMovementScheduleInitialized = false;
#endif
};
