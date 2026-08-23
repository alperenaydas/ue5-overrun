// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestReplicatedActor.generated.h"

UCLASS()
class OVERRUN_API ATestReplicatedActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestReplicatedActor();
	UPROPERTY(Replicated)
	float Counter;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UTextRenderComponent> Label;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void OnBumpPressed();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Bump();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShowMessage(const FString& Message);

};
