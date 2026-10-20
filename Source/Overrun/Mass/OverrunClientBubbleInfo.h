// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassClientBubbleInfoBase.h"
#include "OverrunClientBubble.h"
#include "OverrunClientBubbleInfo.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API AOverrunClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()
	
public:
	AOverrunClientBubbleInfo(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;
	
	FOverrunClientBubbleSerializer& GetAgentsSerializer() { return AgentsSerializer; }
	
protected:
	virtual void BeginPlay() override;

	UFUNCTION(Client, Unreliable)
	void ClientReceiveMovement(const FOverrunMovementPacket& Packet);

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:
	UPROPERTY(Replicated, Transient)
	FOverrunClientBubbleSerializer AgentsSerializer;

private:
	double MovementCredit = 0.0;
	uint32 MovementSequence = 0;
	double MovementStatsElapsed = 0.0;
	uint32 MovementRecordsAttempted = 0;
	uint32 MovementPacketsAttempted = 0;
};
