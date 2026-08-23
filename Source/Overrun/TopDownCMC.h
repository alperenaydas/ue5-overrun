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
	
	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual uint8 GetCompressedFlags() const override;
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
	UPROPERTY(EditDefaultsOnly)
	float MaxSprintSpeed = 1000.f;

	
private:
	uint8 bWantsToSprint : 1;
	// if more than 64 corrections in single second, we are underreporting.
	static constexpr int32 CorrectionHistorySize = 64;
	double CorrectionHistory[CorrectionHistorySize];
	int32 CorrectionHistoryIndex = 0;
	
public:
	void SetSprinting(const bool IsSprinting);
	virtual float GetMaxSpeed() const override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	int32 LastSecondCorrectionCount() const;
	float LastComputedCorrectionDistance = 0.f;
	
protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;
	
};
