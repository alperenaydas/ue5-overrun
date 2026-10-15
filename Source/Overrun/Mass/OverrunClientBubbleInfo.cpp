// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Mass/OverrunClientBubbleInfo.h"
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
