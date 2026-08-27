#include "OverrunHUD.h"

#include "TopDownCMC.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"

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

    TArray<TPair<FString, FLinearColor>> Lines;

    if (const APawn* OwningPawn = GetOwningPawn())
    {
        FString RoleText;
        switch (OwningPawn->GetLocalRole())
        {
        case ROLE_Authority:       RoleText = TEXT("AUTHORITY (server)"); break;
        case ROLE_AutonomousProxy: RoleText = TEXT("AUTONOMOUS");         break;
        case ROLE_SimulatedProxy:  RoleText = TEXT("SIMULATED");          break;
        default:                   RoleText = TEXT("NONE");               break;
        }

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
}