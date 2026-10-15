// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Mass/OverrunMassReplicationSubsystem.h"
#include "MassReplicationSubsystem.h"
#include "Engine/World.h"
#include "OverrunClientBubbleInfo.h"

void UOverrunMassReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassReplicationSubsystem>();
}

void UOverrunMassReplicationSubsystem::PostInitialize()
{
	Super::PostInitialize();
	UMassReplicationSubsystem* ReplicationSubsystem = GetWorld()->GetSubsystem<UMassReplicationSubsystem>();

	checkf(ReplicationSubsystem, TEXT("Overrun Mass replication requires UMassReplicationSubsystem"));

	ReplicationSubsystem->RegisterBubbleInfoClass(AOverrunClientBubbleInfo::StaticClass());
}

bool UOverrunMassReplicationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
