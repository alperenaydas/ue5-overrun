#pragma once

#include "OverrunCrowdArrivalTrait.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"

namespace OverrunCrowd
{
	inline void HoldArrivalAtPosition(const FVector& Position, FMassVelocityFragment& Velocity,
		FMassDesiredMovementFragment& Movement, FMassForceFragment& Force, FMassMoveTargetFragment& Target,
		FMassGhostLocationFragment& Ghost, FMassStandingSteeringFragment& Standing)
	{
		Velocity.Value = FVector::ZeroVector;
		Movement.DesiredVelocity = FVector::ZeroVector;
		Force.Value = FVector::ZeroVector;
		Target.DesiredSpeed.Set(0.f);
		Ghost.Location = Position;
		Ghost.Velocity = FVector::ZeroVector;
		Ghost.LastSeenActionID = Target.GetCurrentActionID();
		Standing = FMassStandingSteeringFragment();
		Standing.TargetLocation = Position;
	}

	inline FVector CalculateCloseArrivalVelocity(const FVector& CurrentVelocity, const FVector& NativeVelocity,
		const FVector& ToTarget, const FVector& SeparationAcceleration, double DeltaTime,
		double DesiredSpeed, double MaxSpeed, double MaxAcceleration, const FOverrunCrowdArrivalParameters& Params)
	{
		DeltaTime = FMath::Clamp(DeltaTime, 0.0, 0.1);
		if (DeltaTime <= UE_SMALL_NUMBER)
		{
			return NativeVelocity;
		}
		const double Distance = ToTarget.Size2D();
		const FVector Direction = FVector(ToTarget.X, ToTarget.Y, 0.0).GetSafeNormal();
		const double Remaining = FMath::Max(0.0, Distance - Params.StopDistance);
		const double Speed = FMath::Min(FMath::Min(DesiredSpeed, MaxSpeed), Remaining / Params.ReactionTime);
		const FVector CurrentPlanar(CurrentVelocity.X, CurrentVelocity.Y, 0.0);
		const FVector NativePlanar(NativeVelocity.X, NativeVelocity.Y, 0.0);
		const FVector Separation = SeparationAcceleration.GetClampedToMaxSize(MaxAcceleration);
		const double AvailableAcceleration = FMath::Max(0.0, MaxAcceleration - Separation.Size());
		const FVector CloseAcceleration = Separation + ((Direction * Speed - CurrentPlanar) / Params.ReactionTime)
			.GetClampedToMaxSize(AvailableAcceleration);
		const FVector NativeAcceleration = (NativePlanar - CurrentPlanar) / DeltaTime;
		const double Blend = FMath::Clamp((Params.CloseDistance - Distance) /
			FMath::Max(1.0, double(Params.CloseDistance - Params.StopDistance)), 0.0, 1.0);
		FVector Velocity = (CurrentPlanar + FMath::Lerp(NativeAcceleration, CloseAcceleration, Blend) * DeltaTime)
			.GetClampedToMaxSize(MaxSpeed);
		const double ExcessInwardSpeed = FVector::DotProduct(Velocity, Direction) - Remaining / DeltaTime;
		if (ExcessInwardSpeed > 0.0)
		{
			Velocity -= Direction * ExcessInwardSpeed;
		}
		Velocity.Z = NativeVelocity.Z;
		return Velocity;
	}
}
