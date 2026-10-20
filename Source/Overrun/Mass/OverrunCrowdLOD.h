#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"

namespace OverrunCrowd
{
	struct FLODSettings
	{
		double EnterDistance = 1800.0;
		double ExitDistance = 2200.0;
		double CriticalDistance = 600.0;
		double MinimumHoldTime = 1.0;
		double RetentionDistance = 150.0;
		int32 HighPerPlayer = 300;
	};

	struct FLODCandidate
	{
		FMassEntityHandle Entity;
		int32 PlayerIndex = INDEX_NONE;
		double Distance = TNumericLimits<double>::Max();
		double LastModeChange = -TNumericLimits<double>::Max();
		bool bWasHigh = false;
		bool bHasNativeAvoidance = true;
		bool bWantHigh = false;
		bool bHadHighTag = false;
		bool bHadLowTag = false;
		bool bHadMediumTag = false;
		bool bHadOffTag = false;
	};

	inline int32 SelectHighAgents(TArray<FLODCandidate>& Candidates, int32 PlayerCount, double Now, const FLODSettings& Settings)
	{
		int32 Overflow = 0;
		for (FLODCandidate& Candidate : Candidates)
		{
			Candidate.bWantHigh = false;
		}
		for (int32 Player = 0; Player < PlayerCount; ++Player)
		{
			int32 HighCount = 0;
			TArray<int32> Eligible;
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				FLODCandidate& Candidate = Candidates[Index];
				if (Candidate.PlayerIndex != Player || !Candidate.bHasNativeAvoidance)
				{
					continue;
				}
				const bool bHolding = Now - Candidate.LastModeChange < Settings.MinimumHoldTime;
				const bool bCritical = Candidate.Distance <= Settings.CriticalDistance;
				if (bCritical || (Candidate.bWasHigh && bHolding && Candidate.Distance <= Settings.ExitDistance))
				{
					Candidate.bWantHigh = true;
					++HighCount;
				}
				else if (Candidate.Distance <= (Candidate.bWasHigh ? Settings.ExitDistance : Settings.EnterDistance) &&
					(Candidate.bWasHigh || !bHolding))
				{
					Eligible.Add(Index);
				}
			}
			Eligible.Sort([&Candidates, &Settings](int32 A, int32 B)
			{
				const FLODCandidate& Left = Candidates[A];
				const FLODCandidate& Right = Candidates[B];
				const double LeftScore = Left.Distance - (Left.bWasHigh ? Settings.RetentionDistance : 0.0);
				const double RightScore = Right.Distance - (Right.bWasHigh ? Settings.RetentionDistance : 0.0);
				return LeftScore != RightScore ? LeftScore < RightScore : Left.Entity < Right.Entity;
			});
			for (int32 Slot = 0; Slot < Eligible.Num() && HighCount < Settings.HighPerPlayer; ++Slot)
			{
				Candidates[Eligible[Slot]].bWantHigh = true;
				++HighCount;
			}
			Overflow += FMath::Max(0, HighCount - Settings.HighPerPlayer);
		}
		return Overflow;
	}
}
