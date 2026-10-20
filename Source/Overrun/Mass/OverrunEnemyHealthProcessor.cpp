#include "OverrunEnemyHealthProcessor.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "OverrunEnemyDamageSubsystem.h"
#include "OverrunEnemyHealthFragment.h"
#include "OverrunMassProbeTag.h"

UOverrunMassHealthProcessor::UOverrunMassHealthProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);

	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
}

void UOverrunMassHealthProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FOverrunMassProbeTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FOverrunEnemyHealthFragment>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UOverrunEnemyDamageSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UOverrunMassHealthProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UOverrunEnemyDamageSubsystem& DamageSubsystem = Context.GetMutableSubsystemChecked<UOverrunEnemyDamageSubsystem>();
	const TArray<FOverrunEnemyDamageCommand> Commands = DamageSubsystem.ConsumePendingDamage();
	if (Commands.IsEmpty())
	{
		return;
	}
	EntityQuery.ForEachEntityChunk(Context, [&Commands](FMassExecutionContext& Context)
	{
		const TArrayView<FOverrunEnemyHealthFragment> Healths = Context.GetMutableFragmentView<FOverrunEnemyHealthFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FOverrunEnemyHealthFragment& HealthFragment = Healths[EntityIt];
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			for (const FOverrunEnemyDamageCommand& Command : Commands)
			{
				if (Command.TargetEntity != Entity)
				{
					continue;
				}
				if (HealthFragment.Health <= 0.f)
				{
					continue;
				}

				const float PreviousHealth = HealthFragment.Health;
				HealthFragment.Health = FMath::Clamp(PreviousHealth - Command.Damage, 0.f, HealthFragment.MaxHealth);

				if (HealthFragment.Health < PreviousHealth)
				{
					++HealthFragment.HitSerial;
				}
				
				if (HealthFragment.Health <= 0.f)
				{
					Context.Defer().DestroyEntity(Entity);
					break;
				}
			}
		}
	});
}
