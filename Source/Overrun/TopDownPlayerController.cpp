// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

ATopDownPlayerController::ATopDownPlayerController()
{
	bShowMouseCursor = true;
}

void ATopDownPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture).SetHideCursorDuringCapture(false));
}

void ATopDownPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (AimStickAction)
		{
			// Triggered supplies the analogue value each frame; Completed is the only event fired on release.
			EIC->BindAction(AimStickAction, ETriggerEvent::Triggered, this, &ATopDownPlayerController::OnAimStick);
			EIC->BindAction(AimStickAction, ETriggerEvent::Completed, this,
			                &ATopDownPlayerController::OnAimStickCompleted);
		}
	}
}

// PlayerTick runs after input processing, so CachedStickInput is current this frame.
void ATopDownPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Local-only: remote controllers on a listen server have no viewport or cursor.
	if (!IsLocalPlayerController() || GetPawn() == nullptr)
	{
		return;
	}

	float Yaw = 0.f;
	if (CachedStickInput.SizeSquared() > StickDeadZone * StickDeadZone)
	{
		// Stick X is horizontal, Y is vertical -> world (+X forward, +Y right), hence the swap.
		Yaw = FVector(CachedStickInput.Y, CachedStickInput.X, 0.f).Rotation().Yaw;
	}
	else
	{
		FVector WorldLocation;
		FVector WorldDirection;
		// Cursor is absolute screen state, not a delta stream, so it's polled rather than bound to an action.
		if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
		{
			// Returns false for a frame or two at startup before the viewport exists.
			return;
		}
		// With an up normal, WorldDirection.Z IS the intersection denominator. Near-zero = ray parallel to the plane = NaN, which would poison control rotation and the move stream.
		if (FMath::Abs(WorldDirection.Z) < KINDA_SMALL_NUMBER)
		{
			return;
		}

		FVector PawnLocation = GetPawn()->GetActorLocation();

		// Plane rides with the pawn so aim doesn't drift on slopes. Height is measured from capsule center, not the feet.
		FVector PlaneOrigin = PawnLocation + FVector(0.f, 0.f, AimPlaneHeight);

		FVector IntersectionPoint = FMath::LinePlaneIntersection(WorldLocation,
		                                                         WorldLocation + WorldDirection * 10000.f, PlaneOrigin,
		                                                         FVector::UpVector);
		FVector AimDirection = IntersectionPoint - PawnLocation;
		AimDirection.Z = 0.f;
		
		if (AimDirection.SizeSquared() > MinAimDistance * MinAimDistance)
		{
			// Flatten: the hit point sits at plane height, so an unflattened vector carries a pitch component.
			Yaw = AimDirection.Rotation().Yaw;
		}
		else
		{
			Yaw = LastAimYaw;
		}
	}
	
	LastAimYaw = Yaw;
	// Pitch/roll zeroed - a top-down character shouldn't tilt, and stray pitch would ride along in every move packet.
	SetControlRotation(FRotator(0.f, Yaw, 0.f));
}

void ATopDownPlayerController::OnAimStick(const FInputActionValue& Value)
{
	CachedStickInput = Value.Get<FVector2D>();
}

void ATopDownPlayerController::OnAimStickCompleted(const FInputActionValue& Value)
{
	CachedStickInput = FVector2D::ZeroVector;
}
