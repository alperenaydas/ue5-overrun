#pragma once

#include "MassClientBubbleHandler.h"
#include "MassReplicationTransformHandlers.h"
#include "MassClientBubbleSerializerBase.h"
#include "OverrunReplicatedAgent.h"
#include "OverrunMovementPacket.h"
#include "OverrunClientBubble.generated.h"

class OVERRUN_API FOverrunClientBubbleHandler : public TClientBubbleHandlerBase<FOverrunAgentFastArrayItem>
{
public:
	virtual void Reset() override;

#if UE_REPLICATION_COMPILE_SERVER_CODE
	void QueueMovement(
		FMassReplicatedAgentHandle Handle,
		const FTransform& Transform,
		double SampleTime);

	bool BuildMovementPacket(
		double Now,
		int32 MaxRecords,
		FOverrunMovementPacket& OutPacket);
#endif

#if UE_REPLICATION_COMPILE_CLIENT_CODE
	void ApplyMovement(const FOverrunMovementPacket& Packet);
#endif
	using FTransformHandler = TMassClientBubbleTransformHandler<FOverrunAgentFastArrayItem>;

	FOverrunClientBubbleHandler();

#if UE_REPLICATION_COMPILE_SERVER_CODE
	FTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }
	void SetHitSerial(FMassReplicatedAgentHandle Handle, uint32 HitSerial);
#endif

protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize) override;
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;
#endif

	FTransformHandler TransformHandler;

private:
#if UE_REPLICATION_COMPILE_SERVER_CODE
	int32 MovementCursor = 0;
#endif

#if UE_REPLICATION_COMPILE_CLIENT_CODE
	struct FMovementBinding
	{
		FMassNetworkID NetID;
		uint32 LastSequence = 0;
		uint32 LastSampleTimeMs = 0;
		bool bHasSequence = false;
	};

	TMap<int32, FMovementBinding> MovementBindings;
#endif
};

USTRUCT()
struct FOverrunClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FOverrunClientBubbleSerializer()
	{
		Bubble.Initialize(Agents, *this);
	};
		
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FOverrunAgentFastArrayItem, FOverrunClientBubbleSerializer>(Agents, DeltaParams, *this);
	}

public:
	FOverrunClientBubbleHandler Bubble;

protected:
	UPROPERTY(Transient)
	TArray<FOverrunAgentFastArrayItem> Agents;
};

template<>
struct TStructOpsTypeTraits<FOverrunClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FOverrunClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};
