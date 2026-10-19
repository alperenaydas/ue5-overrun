#pragma once

#include "MassClientBubbleHandler.h"
#include "MassReplicationTransformHandlers.h"
#include "MassClientBubbleSerializerBase.h"
#include "OverrunReplicatedAgent.h"
#include "OverrunClientBubble.generated.h"

class OVERRUN_API FOverrunClientBubbleHandler : public TClientBubbleHandlerBase<FOverrunAgentFastArrayItem>
{
public:
	using FTransformHandler = TMassClientBubbleTransformHandler<FOverrunAgentFastArrayItem>;

	FOverrunClientBubbleHandler();

#if UE_REPLICATION_COMPILE_SERVER_CODE
	FTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }
	void SetHitSerial(FMassReplicatedAgentHandle Handle, uint32 HitSerial);
#endif

protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;
#endif

	FTransformHandler TransformHandler;
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