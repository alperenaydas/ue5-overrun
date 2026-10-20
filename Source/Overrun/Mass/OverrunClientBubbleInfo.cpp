// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Mass/OverrunClientBubbleInfo.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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

	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const bool bPossessionReady = Controller && IsValid(Controller->GetPawn()) &&
		Controller->AcknowledgedPawn == Controller->GetPawn();
	if (!bPossessionReady)
	{
		MovementCredit = 0.0;
		return;
	}

	constexpr int32 HeaderBytes = OverrunMovement::HeaderBytes;
	constexpr int32 RecordBytes = OverrunMovement::RecordBytes;
	constexpr int32 CallAllowanceBytes = 64;
	constexpr int32 MaxPacketsPerTick = 32;
	const double BytesPerSecond = FMath::Min(380000.0, double(Connection->CurrentNetSpeed) * 0.75);
	const UNetDriver* Driver = Connection->GetDriver();
	const double EngineTickRate = GEngine ? GEngine->GetMaxTickRate(0.0f, false) : 0.0;
	const double NetTickRate = Driver ? double(Driver->MaxNetTickRate) : 0.0;
	const double DesiredTickRate = NetTickRate > 0.0
		? (EngineTickRate > 0.0 ? FMath::Min(EngineTickRate, NetTickRate) : NetTickRate)
		: EngineTickRate;
	double BudgetDeltaSeconds = FMath::Clamp(double(DeltaSeconds), 0.0, 0.1);
	if (DesiredTickRate > 0.0)
	{
		BudgetDeltaSeconds = FMath::Min(BudgetDeltaSeconds, 1.0 / DesiredTickRate);
	}
	const double FrameCredit = BytesPerSecond * BudgetDeltaSeconds;
	const double CreditLimit = FMath::Max(double(HeaderBytes + RecordBytes + CallAllowanceBytes), FrameCredit);
	MovementCredit = FMath::Min(CreditLimit, MovementCredit + FrameCredit);

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
		ClientReceiveMovement(Packet);
	}
#endif
}

void AOverrunClientBubbleInfo::ClientReceiveMovement_Implementation(const FOverrunMovementPacket& Packet)
{
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	AgentsSerializer.Bubble.ApplyMovement(Packet);
#endif
}
