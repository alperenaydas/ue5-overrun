#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "OverrunEnemyPresentationTrait.generated.h"

UCLASS(meta = (DisplayName = "Overrun Enemy Presentation"))
class OVERRUN_API UOverrunEnemyPresentationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World,
		FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
