// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownCMC.h"

#include "GameFramework/Character.h"

FSavedMove_TopDown::FSavedMove_TopDown() : Super()
{
	bWantsToSprint = false;
	bWantsToDash = false;
	SavedDashActiveRemaining = 0.f;
	SavedDashRemainingCooldown = 0.f;
}

void FSavedMove_TopDown::Clear()
{
	Super::Clear();
	bWantsToSprint = false;
	bWantsToDash = false;
	SavedDashActiveRemaining = 0.f;
	SavedDashRemainingCooldown = 0.f;
}

// capture (will be predicted)
void FSavedMove_TopDown::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UTopDownCMC* CMC = Cast<UTopDownCMC>(C->GetCharacterMovement()))
	{
		bWantsToSprint = CMC->bWantsToSprint;
		bWantsToDash = CMC->bWantsToDash;
		SavedDashActiveRemaining = CMC->DashActiveRemaining;
		SavedDashRemainingCooldown = CMC->DashRemainingCooldown;
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
		CMC->DashActiveRemaining = SavedDashActiveRemaining;
		CMC->DashRemainingCooldown = SavedDashRemainingCooldown;
	}
}

// if cached move state is combineable with new move state. looks like unreal does not send data all the time so it combines.
// combine check merges consecutive move sets with same intent into one packet, so if sprint differs, can't combine.
bool FSavedMove_TopDown::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_TopDown* SavedMove = static_cast<const FSavedMove_TopDown*>(NewMove.Get());
	if (SavedMove->bWantsToSprint == bWantsToSprint && SavedMove->bWantsToDash == bWantsToDash)
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
		CMC->DashRemainingCooldown = TopDownOldMove->SavedDashRemainingCooldown;
	}
}

FNetworkPredictionData_Client_TopDown::FNetworkPredictionData_Client_TopDown(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement)
{
	
}

FSavedMovePtr FNetworkPredictionData_Client_TopDown::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_TopDown());
}

UTopDownCMC::UTopDownCMC()
{
	for (int i = 0; i < CorrectionHistorySize; i++)
	{
		CorrectionHistory[i] = 0.0;
	}
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
		return bWantsToSprint ? MaxSprintSpeed : Super::GetMaxSpeed();
	}
	return Super::GetMaxSpeed();
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
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
	                                  bHasBase, bBaseRelativePosition,
	                                  ServerMovementMode, ServerGravityDirection);
}

bool UTopDownCMC::CanDash() const
{
	if (MovementMode != MOVE_Walking || DashRemainingCooldown > 0)
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
	DashRemainingCooldown = DashCooldownDuration;
	DashActiveRemaining = DashActiveDuration;
}

void UTopDownCMC::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (DashRemainingCooldown > 0)
		{
			DashRemainingCooldown -= DeltaSeconds;
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
