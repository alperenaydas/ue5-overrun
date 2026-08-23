// Fill out your copyright notice in the Description page of Project Settings.


#include "TestReplicatedActor.h"

#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ATestReplicatedActor::ATestReplicatedActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(48.f);
	
	AutoReceiveInput = EAutoReceiveInput::Player0;
}

// Called when the game starts or when spawned
void ATestReplicatedActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ATestReplicatedActor::OnBumpPressed);
	}
}

// Called every frame
void ATestReplicatedActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && GetOwner() == nullptr)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			SetOwner(PC);
			UE_LOG(LogTemp, Warning, TEXT("[SERVER] Owner set to %s"), *PC->GetName());
		}
	}
	
	if (GetNetMode() == NM_DedicatedServer || !Label)
	{
		return;
	}
	
	FString RoleText;
	switch (GetLocalRole())
	{
	case ROLE_Authority:        RoleText = TEXT("AUTHORITY (server)"); break;
	case ROLE_AutonomousProxy:  RoleText = TEXT("AUTONOMOUS");         break;
	case ROLE_SimulatedProxy:   RoleText = TEXT("SIMULATED");          break;
	default:                    RoleText = TEXT("NONE");               break;
	}
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s\nCounter: %.2f"), *RoleText, Counter)));
}

void ATestReplicatedActor::Multicast_ShowMessage_Implementation(const FString& Message)
{
	const FString Where = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

	UE_LOG(LogTemp, Warning, TEXT("[%s] Multicast: %s"), *Where, *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.f,
			FColor::Cyan,
			FString::Printf(TEXT("[%s] %s"), *Where, *Message));
	}
}

void ATestReplicatedActor::Server_Bump_Implementation()
{
	Counter += 10.f;

	UE_LOG(LogTemp, Warning, TEXT("[SERVER] Server_Bump ran. HasAuthority=%d Counter=%.2f"),
		HasAuthority() ? 1 : 0, Counter);

	Multicast_ShowMessage(FString::Printf(TEXT("Counter bumped to %.2f"), Counter));
}

bool ATestReplicatedActor::Server_Bump_Validate()
{
	return true;
}

void ATestReplicatedActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ATestReplicatedActor, Counter);
}

void ATestReplicatedActor::OnBumpPressed()
{
	UE_LOG(LogTemp, Warning, TEXT("[LOCAL] E pressed. Role=%d"), (int32)GetLocalRole());
	Server_Bump();
}
