// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Mass/OverrunClientBubbleInfo.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AOverrunClientBubbleInfo::AOverrunClientBubbleInfo(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Serializers.Add(&AgentsSerializer);
}

void AOverrunClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(AOverrunClientBubbleInfo, AgentsSerializer, SharedParams);
}

void AOverrunClientBubbleInfo::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SetActorTickEnabled(true);
	}
}

void AOverrunClientBubbleInfo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if UE_REPLICATION_COMPILE_SERVER_CODE
	if (!HasAuthority())
	{
		return;
	}

	UNetConnection* Connection = GetNetConnection();
	if (!Connection || Connection->CurrentNetSpeed <= 0)
	{
		return;
	}

	constexpr int32 HeaderBytes = OverrunMovement::HeaderBytes;
	constexpr int32 RecordBytes = OverrunMovement::RecordBytes;
	constexpr int32 CallAllowanceBytes = 64;
	constexpr int32 MaxPacketsPerTick = 32;
	constexpr double MaxCredit = MaxPacketsPerTick *
		(HeaderBytes + RecordBytes * OverrunMovement::MaxRecords + CallAllowanceBytes);

	const double BytesPerSecond = FMath::Min(380000.0, double(Connection->CurrentNetSpeed) * 0.75);
	MovementCredit = FMath::Min(MaxCredit, MovementCredit +
		BytesPerSecond * FMath::Clamp(double(DeltaSeconds), 0.0, 0.1));

	const int32 MaxRecordsByBunch = FMath::Clamp(
		(Connection->GetMaxSingleBunchSizeBits() - 1024 - OverrunMovement::HeaderBits) /
			OverrunMovement::RecordBits,
		0, OverrunMovement::MaxRecords);
	const double Now = GetWorld()->GetTimeSeconds();

	for (int32 Attempt = 0; Attempt < MaxPacketsPerTick; ++Attempt)
	{
		if (!Connection->IsNetReady())
		{
			break;
		}

		const int32 MaxRecordsByCredit = FMath::Max(0,
			int32((MovementCredit - HeaderBytes - CallAllowanceBytes) / RecordBytes));
		const int32 Limit = FMath::Min(MaxRecordsByBunch, MaxRecordsByCredit);
		if (Limit <= 0)
		{
			break;
		}

		FOverrunMovementPacket Packet;
		if (!AgentsSerializer.Bubble.BuildMovementPacket(Now, Limit, Packet))
		{
			break;
		}

		Packet.Sequence = ++MovementSequence;
		MovementCredit -= HeaderBytes + RecordBytes * Packet.Records.Num() + CallAllowanceBytes;
		MovementRecordsAttempted += uint32(Packet.Records.Num());
		++MovementPacketsAttempted;
		ClientReceiveMovement(Packet);
	}

	MovementStatsElapsed += DeltaSeconds;
	if (MovementStatsElapsed >= 1.0)
	{
		UE_LOG(LogTemp, Display,
			TEXT("OverrunMovement: agents=%d attempted=%.0f records/s packets=%.0f/s netspeed=%d"),
			AgentsSerializer.Bubble.GetAgents().Num(),
			double(MovementRecordsAttempted) / MovementStatsElapsed,
			double(MovementPacketsAttempted) / MovementStatsElapsed,
			Connection->CurrentNetSpeed);
		MovementStatsElapsed = 0.0;
		MovementRecordsAttempted = 0;
		MovementPacketsAttempted = 0;
	}
#endif
}

void AOverrunClientBubbleInfo::ClientReceiveMovement_Implementation(const FOverrunMovementPacket& Packet)
{
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	AgentsSerializer.Bubble.ApplyMovement(Packet);
#endif
}
