// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownCharacter.h"

#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "KismetAnimationLibrary.h"
#include "../AbilitySystem/OverrunGameplayTags.h"
#include "../OverrunHelper.h"
#include "../Debug/OverrunNetDebug.h"
#include "../Movement/TopDownCMC.h"
#include "../Player/TopDownPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ATopDownCharacter::ATopDownCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTopDownCMC>(ACharacter::CharacterMovementComponentName))
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	CameraBoom->SetupAttachment(RootComponent);
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->TargetArmLength = 1000.f;
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera->bUsePawnControlRotation = false;


	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationPitch = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 1080.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}

// Called when the game starts or when spawned
void ATopDownCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ATopDownCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Client-side, after possession resolves, on the machine that owns this pawn.
	// BeginPlay is too early (controller link may be unresolved); OnPossess is server-only.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}


void ATopDownCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATopDownCharacter::OnMoveAction);
		}
		if (SprintAction)
		{
			EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ATopDownCharacter::OnSprintActionStarted);
			EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ATopDownCharacter::OnSprintActionCompleted);
		}
		if (DashAction)
		{
			EIC->BindAction(DashAction, ETriggerEvent::Started, this, &ATopDownCharacter::OnDashActionStarted);
		}
	}
}

UAbilitySystemComponent* ATopDownCharacter::GetAbilitySystemComponent() const
{
	if (const ATopDownPlayerState* PS = Cast<ATopDownPlayerState>(GetPlayerState()))
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}

void ATopDownCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ensureMsgf(ASC, TEXT("PossessedBy: no ASC on %s — PlayerState missing or not ATopDownPlayerState (%s)"), *GetName(), *GetNameSafe(GetPlayerState())))
	{
		ASC->InitAbilityActorInfo(GetPlayerState(), this);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogAbilitySystemComponent, Display, TEXT("Ability System Component initialized for %s on server which's role is %s and id is %d."), *GetFullName(), *GetRoleName(GetLocalRole()), GetPlayerState()->GetPlayerId());
#endif
		for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
		{
			if (ensureMsgf(Ability, TEXT("PossessedBy: Missing ability on %s. Double check the blueprint."), *GetName()))
			{
				// Ability may be given on previous PossessedBy so this is double guard.
				if (!ASC->FindAbilitySpecFromClass(Ability))
				{
					FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Ability);
					ASC->GiveAbility(AbilitySpec);
				}
			}
		}
	}
}

void ATopDownCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->InitAbilityActorInfo(GetPlayerState(), this);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogAbilitySystemComponent, Display, TEXT("Ability System Component initialized for %s on client which's role is %s and id is %d."), *GetFullName(), *GetRoleName(GetLocalRole()), GetPlayerState()->GetPlayerId());
#endif
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRep_PlayerState: ASC not ready on %s — PlayerState actor not replicated yet, expecting a later notify."), *GetName());
	}
}

double ATopDownCharacter::GetGroundSpeed() const
{
	return GetVelocity().Size2D();
}

float ATopDownCharacter::GetLocomotionDirection() const
{
	return UKismetAnimationLibrary::CalculateDirection(GetVelocity(), GetActorRotation());
}

void ATopDownCharacter::OnMoveAction(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();

	// World axes on purpose: the camera boom's yaw is pinned to 0, so screen axes and
	// world axes coincide. If the boom yaw ever changes, both this and the aim-stick
	// vector in ATopDownPlayerController must be rotated by the boom's yaw.
	AddMovementInput(FVector::ForwardVector, MoveInput.Y);
	AddMovementInput(FVector::RightVector, MoveInput.X);
}

void ATopDownCharacter::OnSprintActionStarted(const FInputActionValue& Value)
{
	OverrunHelper::TryActivateAbilityByTag(GetAbilitySystemComponent(), TAG_Ability_Sprint);
}

void ATopDownCharacter::OnSprintActionCompleted(const FInputActionValue& Value)
{
	if (UTopDownCMC* CMC = Cast<UTopDownCMC>(GetCharacterMovement()))
	{
		CMC->SetSprinting(false);
	}
}

void ATopDownCharacter::OnDashActionStarted(const FInputActionValue& Value)
{
	OverrunHelper::TryActivateAbilityByTag(GetAbilitySystemComponent(), TAG_Ability_Dash);
}