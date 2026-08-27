// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TopDownCMC.generated.h"

class UTopDownCMC;

class FSavedMove_TopDown : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;
	
	FSavedMove_TopDown();
	
	uint8 bWantsToSprint : 1;
	uint8 bWantsToDash : 1;
	float SavedDashActiveRemaining = 0.f;
	float SavedDashRemainingCooldown = 0.f;
	float SavedCurrentStamina = 0.f;
	float SavedStaminaRecoveryRemainingCooldown = 0.f;
	uint8 bStaminaExhausted : 1;
	
	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual uint8 GetCompressedFlags() const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void SetInitialPosition(ACharacter* C) override;
};

class FNetworkPredictionData_Client_TopDown : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;
	
	FNetworkPredictionData_Client_TopDown(const UCharacterMovementComponent& ClientMovement);
	
	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 * 
 */

UCLASS()
class OVERRUN_API UTopDownCMC : public UCharacterMovementComponent
{
	GENERATED_BODY()
	friend class FSavedMove_TopDown;
public:
	UTopDownCMC();
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float MaxSprintSpeed = 1000.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float MaxDashBrakingDeceleration = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float DashSpeed = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float DashCooldownDuration = 1.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float DashActiveDuration = 0.2f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float DashStaminaCost = 25.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float MaxStamina = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float StaminaSprintDrainRate = 25.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float StaminaRecoveryRate = 10.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float StaminaRecoveryDelay = 3.f;
	UPROPERTY(EditDefaultsOnly, Category="Custom Movement")
	float StaminaExhaustionRecoveryThreshold = 20.f;
	
private:
	uint8 bWantsToSprint : 1;
	uint8 bWantsToDash : 1;
	float DashRemainingCooldown = 0.f;
	float DashActiveRemaining = 0.f;
	// if more than 64 corrections in single second, we are underreporting.
	static constexpr int32 CorrectionHistorySize = 64;
	double CorrectionHistory[CorrectionHistorySize];
	int32 CorrectionHistoryIndex = 0;
	float CurrentStamina = 0.f;
	float StaminaRecoveryRemainingCooldown = 0.f;
	uint8 bStaminaExhausted : 1;
	
public:
	void SetSprinting(const bool IsSprinting);
	void TriggerDashing();
	virtual float GetMaxSpeed() const override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	int32 LastSecondCorrectionCount() const;
	float LastComputedCorrectionDistance = 0.f;
	float GetCurrentStamina() const;
	
protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;
	bool CanDash() const;
	void Dash();
	bool IsSprinting() const;
	void SpendStamina(float StaminaCost);
	void RecoverStamina(float DeltaSeconds);
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;
	virtual void BeginPlay() override;
};
