#include "OverrunHUD.h"

#include "../Movement/TopDownCMC.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "OverrunNetDebug.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "Overrun/AbilitySystem/OverrunAttributeSet.h"
#include "Overrun/Character/TopDownCharacter.h"

void AOverrunHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas || !GEngine)
    {
        return;
    }

    UFont* Font = GEngine->GetMediumFont();
    if (!Font)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    TArray<TPair<FString, FLinearColor>> Lines;

    if (const APawn* OwningPawn = GetOwningPawn())
    {
        const FString RoleText = GetRoleName(OwningPawn->GetLocalRole());
        Lines.Emplace(FString::Printf(TEXT("Role:        %s"), *RoleText), FLinearColor::White);
        Lines.Emplace(FString::Printf(TEXT("Velocity:    %.0f"), OwningPawn->GetVelocity().Size2D()),
                      FLinearColor::White);

        if (const UTopDownCMC* CMC = Cast<UTopDownCMC>(OwningPawn->GetMovementComponent()))
        {
            const int32 Corrections = CMC->LastSecondCorrectionCount();

            FLinearColor CorrectionColor = FLinearColor::Green;
            if (Corrections > 10)     { CorrectionColor = FLinearColor::Red; }
            else if (Corrections > 2) { CorrectionColor = FLinearColor::Yellow; }

            Lines.Emplace(FString::Printf(TEXT("Corrections: %d /s"), Corrections), CorrectionColor);
            Lines.Emplace(FString::Printf(TEXT("Last error distance %.2f cm"), CMC->LastComputedCorrectionDistance), CorrectionColor);
            Lines.Emplace(FString::Printf(TEXT("Movement Mode: %s"), *CMC->GetMovementName()), FLinearColor::White);
            Lines.Emplace(FString::Printf(TEXT("Stamina: %.2f/%.2f"), CMC->GetCurrentStamina(), CMC->MaxStamina), FLinearColor::White);
            if (const ATopDownCharacter* TopDownCharacter = Cast<ATopDownCharacter>(OwningPawn))
            {
                if (const UAbilitySystemComponent* ASC = TopDownCharacter->GetAbilitySystemComponent())
                {
                    Lines.Emplace(FString::Printf(TEXT("Attr Stamina: %.2f/%.2f"), ASC->GetNumericAttribute(UOverrunAttributeSet::GetStaminaAttribute()), CMC->MaxStamina), FLinearColor::White);
                }
            }
        }
        for (TActorIterator<ATopDownCharacter> Iterator(GetWorld()); Iterator; ++Iterator)
        {
            if (const ATopDownCharacter* TopDownCharacter = *Iterator)
            {
                if (TopDownCharacter == OwningPawn) { continue; }
                if (const UAbilitySystemComponent* ASC = TopDownCharacter->GetAbilitySystemComponent())
                {
                    if (const APlayerState* PS = TopDownCharacter->GetPlayerState())
                    {
                        Lines.Emplace(FString::Printf(TEXT("Stamina of %d: %.2f"), PS->GetPlayerId(), ASC->GetNumericAttribute(UOverrunAttributeSet::GetStaminaAttribute())), FLinearColor::White);
                    }
                }
            }
        }
    }

    if (const APlayerController* PC = GetOwningPlayerController())
    {
        if (PC->PlayerState)
        {
            Lines.Emplace(FString::Printf(TEXT("Ping:        %.0f ms"),
                          PC->PlayerState->GetPingInMilliseconds()), FLinearColor::White);
        }
    }

    if (Lines.Num() == 0)
    {
        return;
    }

    const float Margin      = 20.f;
    const float Padding     = 10.f;
    const float LineHeight  = 22.f;
    const float PanelWidth  = 340.f;
    const float PanelHeight = Padding * 2.f + LineHeight * Lines.Num();

    FCanvasTileItem Panel(FVector2D(Margin, Margin),
                          FVector2D(PanelWidth, PanelHeight),
                          FLinearColor(0.f, 0.f, 0.f, 0.6f));
    Panel.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Panel);

    float Y = Margin + Padding;
    for (const TPair<FString, FLinearColor>& Line : Lines)
    {
        FCanvasTextItem TextItem(FVector2D(Margin + Padding, Y),
                                 FText::FromString(Line.Key), Font, Line.Value);
        TextItem.EnableShadow(FLinearColor::Black);
        Canvas->DrawItem(TextItem);
        Y += LineHeight;
    }
#endif
}
