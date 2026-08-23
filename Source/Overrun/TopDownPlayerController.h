// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TopDownPlayerController.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
/**
 * 
 */
UCLASS()
class OVERRUN_API ATopDownPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATopDownPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnAimStick(const FInputActionValue& Value);
	void OnAimStickCompleted(const FInputActionValue& Value);

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AimStickAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float AimPlaneHeight = 50.f;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float StickDeadZone = 0.25f;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float MinAimDistance = 0.1f;

private:
	// Last non-neutral right-stick value; zeroed on Completed so the mouse path can resume.
	FVector2D CachedStickInput = FVector2D::ZeroVector;
	float LastAimYaw = 0.f;
};
