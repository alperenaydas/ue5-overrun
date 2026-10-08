// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Debug/OverrunDebugHealthWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/TextBlock.h"

void UOverrunDebugHealthWidget::RefreshDebugVisibility()
{
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Overrun.Debug.Health"));
	SetVisibility(CVar && CVar->GetInt() == 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UOverrunDebugHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bConsoleSinkRegistered)
	{
		ConsoleVariableSinkHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(
			FConsoleCommandDelegate::CreateUObject(this, &UOverrunDebugHealthWidget::RefreshDebugVisibility));
		bConsoleSinkRegistered = true;
	}

	BindHealthDelegates();
	RefreshHealthText();
	RefreshDebugVisibility();
}

void UOverrunDebugHealthWidget::NativeDestruct()
{
	if (bConsoleSinkRegistered)
	{
		IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(ConsoleVariableSinkHandle);
		bConsoleSinkRegistered = false;
	}
	UnbindHealthDelegates();
	Super::NativeDestruct();
}

void UOverrunDebugHealthWidget::RefreshHealthText()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get(); ASC && HealthText)
	{
		FText HealthValueText = FText::AsNumber(ASC->GetNumericAttribute(HealthAttribute));
		FText MaxHealthValueText = FText::AsNumber(ASC->GetNumericAttribute(MaxHealthAttribute));
		FText CombinedHealthText = FText::Format(FText::FromString("{0} / {1}"), HealthValueText, MaxHealthValueText);
		HealthText->SetText(CombinedHealthText);
	}
	else if (HealthText)
	{
		HealthText->SetText(FText::GetEmpty());
	}
}

void UOverrunDebugHealthWidget::HealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealthText();
}

void UOverrunDebugHealthWidget::InitializeFromASC(UAbilitySystemComponent* InASC,
                                                  const FGameplayAttribute& InHealthAttribute,
                                                  const FGameplayAttribute& InMaxHealthAttribute)
{
	// Attribute sets may also become available after the ASC itself.
	if (InASC && (!InHealthAttribute.IsValid() || !InMaxHealthAttribute.IsValid()
		|| !InASC->HasAttributeSetForAttribute(InHealthAttribute)
		|| !InASC->HasAttributeSetForAttribute(InMaxHealthAttribute)))
	{
		InASC = nullptr;
	}

	if (!AbilitySystemComponent.IsStale() && AbilitySystemComponent.Get() == InASC
		&& HealthAttribute == InHealthAttribute && MaxHealthAttribute == InMaxHealthAttribute
		&& (!InASC || (HealthChangedDelegateHandle.IsValid() && MaxHealthChangedDelegateHandle.IsValid())))
	{
		return;
	}

	UnbindHealthDelegates();
	AbilitySystemComponent = InASC;
	HealthAttribute = InHealthAttribute;
	MaxHealthAttribute = InMaxHealthAttribute;
	BindHealthDelegates();
	RefreshHealthText();
}

void UOverrunDebugHealthWidget::BindHealthDelegates()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		if (!HealthChangedDelegateHandle.IsValid())
		{
			HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(HealthAttribute).AddUObject(
				this, &UOverrunDebugHealthWidget::HealthChanged);
		}
		if (!MaxHealthChangedDelegateHandle.IsValid())
		{
			MaxHealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(MaxHealthAttribute).
			                                      AddUObject(this, &UOverrunDebugHealthWidget::HealthChanged);
		}
	}
}

void UOverrunDebugHealthWidget::UnbindHealthDelegates()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(HealthAttribute).Remove(HealthChangedDelegateHandle);
		}
		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(MaxHealthAttribute).Remove(MaxHealthChangedDelegateHandle);
		}
	}
	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
}
