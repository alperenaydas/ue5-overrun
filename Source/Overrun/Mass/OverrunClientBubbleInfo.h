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
	
	FOverrunClientBubbleSerializer& GetAgentsSerializer() { return AgentsSerializer; }
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:
	UPROPERTY(Replicated, Transient)
	FOverrunClientBubbleSerializer AgentsSerializer;
};
