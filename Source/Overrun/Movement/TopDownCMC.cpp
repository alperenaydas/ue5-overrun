// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownCMC.h"

#include "GameFramework/Character.h"

FSavedMove_TopDown::FSavedMove_TopDown() : Super()
{
	bWantsToSprint = false;
	bWantsToDash = false;
	SavedDashActiveRemaining = 0.f;
	SavedDashRemainingCooldown = 0.f;
	SavedCurrentStamina = 0.f;
	SavedStaminaRecoveryRemainingCooldown = 0.f;
	bStaminaExhausted = false;
}

void FSavedMove_TopDown::Clear()
{
	Super::Clear();
	bWantsToSprint = false;
	bWantsToDash = false;
	SavedDashActiveRemaining = 0.f;
	SavedDashRemainingCooldown = 0.f;
	SavedCurrentStamina = 0.f;
	SavedStaminaRecoveryRemainingCooldown = 0.f;
	bStaminaExhausted = false;
}

// capture (will be predicted)
void FSavedMove_TopDown::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UTopDownCMC* CMC = Cast<UTopDownCMC>(C->GetCharacterMovement()))
	{
		bWantsToSprint = CMC->bWantsToSprint;
		bWantsToDash = CMC->bWantsToDash;
		// impulse timing inside a merged delta time can't be restored. thats why we need force no combine.
		if (bWantsToDash)
		{
			bForceNoCombine = true;
		}
	}
}

// restore (reconciliation)
void FSavedMove_TopDown::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UTopDownCMC* CMC = Cast<UTopDownCMC>(C->GetCharacterMovement()))
	{
		CMC->bWantsToSprint = bWantsToSprint;
		CMC->bWantsToDash = bWantsToDash;
	}
}

// if cached move state is combineable with new move state. looks like unreal does not send data all the time so it combines.
// combine check merges consecutive move sets with same intent into one packet, so if sprint differs, can't combine.
bool FSavedMove_TopDown::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_TopDown* SavedMove = static_cast<const FSavedMove_TopDown*>(NewMove.Get());
	if (SavedMove->bWantsToSprint == bWantsToSprint && 
		SavedMove->bWantsToDash == bWantsToDash && 
		SavedMove->bStaminaExhausted == bStaminaExhausted)
	{
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}
		
	return false;
}

// serialization
uint8 FSavedMove_TopDown::GetCompressedFlags() const
{
	uint8 SuperFlags = Super::GetCompressedFlags();
	if (bWantsToSprint)
	{
		SuperFlags |= FLAG_Custom_0;
	}
	if (bWantsToDash)
	{
		SuperFlags |= FLAG_Custom_1;
	}
	return SuperFlags;
}

void FSavedMove_TopDown::CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter,
	APlayerController* PC, const FVector& OldStartLocation)
{
	// this line restores all engine specific states. velocity, position, rotation. does not restore anything related
	// to our custom saved move. that's why we need to override it and restore our own values for cooldowns.
	FSavedMove_Character::CombineWith(OldMove, InCharacter, PC, OldStartLocation);
	UTopDownCMC* CMC = Cast<UTopDownCMC>(InCharacter->GetCharacterMovement());
	if (CMC)
	{
		const FSavedMove_TopDown* TopDownOldMove = static_cast<const FSavedMove_TopDown*>(OldMove);
		CMC->DashActiveRemaining = TopDownOldMove->SavedDashActiveRemaining;
		CMC->DashCooldownRemaining = TopDownOldMove->SavedDashRemainingCooldown;
		CMC->CurrentStamina = TopDownOldMove->SavedCurrentStamina;
		CMC->StaminaRecoveryRemainingCooldown = TopDownOldMove->SavedStaminaRecoveryRemainingCooldown;
		CMC->bStaminaExhausted = TopDownOldMove->bStaminaExhausted;
	}
}

void FSavedMove_TopDown::SetInitialPosition(ACharacter* C)
{
	FSavedMove_Character::SetInitialPosition(C);
	if (UTopDownCMC* CMC = Cast<UTopDownCMC>(C->GetCharacterMovement()))
	{
		SavedDashActiveRemaining = CMC->DashActiveRemaining;
		SavedDashRemainingCooldown = CMC->DashCooldownRemaining;
		SavedCurrentStamina = CMC->CurrentStamina;
		SavedStaminaRecoveryRemainingCooldown = CMC->StaminaRecoveryRemainingCooldown;
		bStaminaExhausted = CMC->bStaminaExhausted;
	}
}

FNetworkPredictionData_Client_TopDown::FNetworkPredictionData_Client_TopDown(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement)
{
	
}

FSavedMovePtr FNetworkPredictionData_Client_TopDown::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_TopDown());
}

FCharacterMoveResponseDataContainerWithTopDownAdditions::FCharacterMoveResponseDataContainerWithTopDownAdditions() : Super()
{
	
}

void FCharacterMoveResponseDataContainerWithTopDownAdditions::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	FCharacterMoveResponseDataContainer::ServerFillResponseData(CharacterMovement, PendingAdjustment);
	// Custom fields are sampled in ServerMoveHandleClientError, at the moment the adjustment is created — so they
	// match the adjustment's timestamp. Do NOT resample live state here: by send time the server may have simulated 
	// past the corrected move, and time-inconsistent data re-poisons every correction.
}

bool FCharacterMoveResponseDataContainerWithTopDownAdditions::Serialize(UCharacterMovementComponent& CharacterMovement,
	FArchive& Ar, UPackageMap* PackageMap)
{
	if (!FCharacterMoveResponseDataContainer::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}
	if (IsCorrection())
	{
		Ar << ResponseCurrentStamina;
		Ar << ResponseStaminaRecoveryRemainingCooldown;
		Ar << ResponseDashActiveRemaining;
		Ar << ResponseDashCooldownRemaining;
		Ar.SerializeBits(&bResponseStaminaExhausted, 1);
	}
	return !Ar.IsError();
	
}

UTopDownCMC::UTopDownCMC()
{
	for (int i = 0; i < CorrectionHistorySize; i++)
	{
		CorrectionHistory[i] = 0.0;
	}
	bWantsToSprint = false;
	bWantsToDash = false;
	bStaminaExhausted = false;
	SetMoveResponseDataContainer(TopDownMoveResponseDataContainer);
}

void UTopDownCMC::SetSprinting(const bool IsSprinting)
{
	bWantsToSprint = IsSprinting;
}

void UTopDownCMC::TriggerDashing()
{
	bWantsToDash = true;
}

float UTopDownCMC::GetMaxSpeed() const
{
	if (MovementMode == MOVE_Walking)
	{
		if (DashActiveRemaining > 0.f)
		{
			return DashSpeed;
		}
		return IsSprinting() ? MaxSprintSpeed : Super::GetMaxSpeed();
	}
	return Super::GetMaxSpeed();
}

float UTopDownCMC::GetCurrentStamina() const
{
	return CurrentStamina;
}

void UTopDownCMC::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	// need to explicitly set bWantsToSprint since Super does not. 
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToDash = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

void UTopDownCMC::OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
	bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	LastComputedCorrectionDistance = FVector::Distance(UpdatedComponent->GetComponentLocation(), NewLocation);
	if (LastComputedCorrectionDistance > 0.f)
	{
		CorrectionHistory[CorrectionHistoryIndex] = FPlatformTime::Seconds(); // Save the time to show the count of last second
		CorrectionHistoryIndex = (CorrectionHistoryIndex + 1) % CorrectionHistorySize;
	}
	CurrentStamina = TopDownMoveResponseDataContainer.ResponseCurrentStamina;
	StaminaRecoveryRemainingCooldown = TopDownMoveResponseDataContainer.ResponseStaminaRecoveryRemainingCooldown;
	DashActiveRemaining = TopDownMoveResponseDataContainer.ResponseDashActiveRemaining;
	DashCooldownRemaining = TopDownMoveResponseDataContainer.ResponseDashCooldownRemaining;
	bStaminaExhausted = TopDownMoveResponseDataContainer.bResponseStaminaExhausted;

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
	                                  bHasBase, bBaseRelativePosition,
	                                  ServerMovementMode, ServerGravityDirection);
}

bool UTopDownCMC::CanDash() const
{
	if (MovementMode != MOVE_Walking || DashCooldownRemaining > 0)
	{
		return false;
	}
	// we are not checking 'bStaminaExhausted' because we want to be able to dash even though we are exhausted.
	if (CurrentStamina < DashStaminaCost)
	{
		return false;
	}
	return true;
}

void UTopDownCMC::Dash()
{
	if (!HasValidData())
	{
		return;
	}
	
	FVector DashDirection = FRotator(0.f, GetCharacterOwner()->GetControlRotation().Yaw, 0.f).Vector().GetSafeNormal2D();
	if (Acceleration.SizeSquared2D() > 0)
	{
		DashDirection = Acceleration.GetSafeNormal2D();
	}
	Velocity = DashDirection * DashSpeed;
	DashCooldownRemaining = DashCooldownDuration;
	DashActiveRemaining = DashActiveDuration;
	SpendStamina(DashStaminaCost);
}

bool UTopDownCMC::IsSprinting() const
{
	return bWantsToSprint && !bStaminaExhausted && Acceleration.SizeSquared2D() > 0.f;
}

void UTopDownCMC::SpendStamina(float StaminaCost)
{
	StaminaRecoveryRemainingCooldown = StaminaRecoveryDelay;
	CurrentStamina = FMath::Clamp(CurrentStamina - StaminaCost, 0.f, MaxStamina);
	if (CurrentStamina < KINDA_SMALL_NUMBER)
	{
		bStaminaExhausted = true;
	}
}

void UTopDownCMC::RecoverStamina(float DeltaSeconds)
{
	CurrentStamina = FMath::Clamp(CurrentStamina + StaminaRecoveryRate * DeltaSeconds, 0.f, MaxStamina);
	if (CurrentStamina > StaminaExhaustionRecoveryThreshold)
	{
		bStaminaExhausted = false;
	}
}

void UTopDownCMC::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (IsSprinting())
		{
			SpendStamina(StaminaSprintDrainRate * DeltaSeconds);
		}
		if (StaminaRecoveryRemainingCooldown > 0.f)
		{
			StaminaRecoveryRemainingCooldown -= DeltaSeconds;
		}
		else
		{
			RecoverStamina(DeltaSeconds);
		}
		if (DashCooldownRemaining > 0)
		{
			DashCooldownRemaining -= DeltaSeconds;
		}
		if (DashActiveRemaining > 0)
		{
			DashActiveRemaining -= DeltaSeconds;
		}
		if (bWantsToDash && CanDash())
		{
			Dash();
		}
		bWantsToDash = false;
	}
}

float UTopDownCMC::GetMaxBrakingDeceleration() const
{
	if (MovementMode == MOVE_Walking && DashActiveRemaining > 0)
	{
		return MaxDashBrakingDeceleration;
	}
	return Super::GetMaxBrakingDeceleration();
}

void UTopDownCMC::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (MovementMode == MOVE_Walking && DashActiveRemaining > 0.f)
	{
		// zero friction for dashing
		return Super::ApplyVelocityBraking(DeltaTime, 0.f, BrakingDeceleration);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

void UTopDownCMC::BeginPlay()
{
	Super::BeginPlay();
	CurrentStamina = MaxStamina;
}

void UTopDownCMC::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	Super::ServerMoveHandleClientError(ClientTimeStamp, DeltaTime, Accel, RelativeClientLocation, ClientMovementBase,
	                                   ClientBaseBoneName,
	                                   ClientMovementMode);
	FClientAdjustment Adjustment = GetPredictionData_Server_Character()->PendingAdjustment;
	if (Adjustment.TimeStamp == ClientTimeStamp && !Adjustment.bAckGoodMove)
	{
		TopDownMoveResponseDataContainer.ResponseCurrentStamina = CurrentStamina;
		TopDownMoveResponseDataContainer.ResponseStaminaRecoveryRemainingCooldown = StaminaRecoveryRemainingCooldown;
		TopDownMoveResponseDataContainer.ResponseDashActiveRemaining = DashActiveRemaining;
		TopDownMoveResponseDataContainer.ResponseDashCooldownRemaining = DashCooldownRemaining;
		TopDownMoveResponseDataContainer.bResponseStaminaExhausted = bStaminaExhausted;
	}
}

int32 UTopDownCMC::LastSecondCorrectionCount() const
{
	int32 Count = 0;
	const double Now = FPlatformTime::Seconds();
	for (int32 i = 0; i < CorrectionHistorySize; i++)
	{
		if (CorrectionHistory[i] > Now - 1.0)
		{
			Count++;
		}
	}
	return Count;
}

class FNetworkPredictionData_Client* UTopDownCMC::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UTopDownCMC* MutableThis = const_cast<UTopDownCMC*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_TopDown(*this);
	}
	
	return ClientPredictionData;
}
