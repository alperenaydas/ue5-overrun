#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "TopDownCharacter.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;

UCLASS()
class OVERRUN_API ATopDownCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATopDownCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DashAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DamageAction; // temp for damage tests
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	float DefaultMaxHealth = 100.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DamageEffect;

public:
	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	bool IsDead() const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Locomotion")
	double GetGroundSpeed() const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Locomotion")
	float GetLocomotionDirection() const;
	UFUNCTION(Server, Reliable)
	void ShootWeapon();
	UFUNCTION(NetMulticast, Unreliable) // cause its debug
	void ClientDrawShotDebug(FVector Start, FVector End);
	
	void EnterDeadTransition() const;

private:
	void OnMoveAction(const FInputActionValue& Value);
	void OnSprintActionStarted(const FInputActionValue& Value);
	void OnSprintActionCompleted(const FInputActionValue& Value);
	void OnDashActionStarted(const FInputActionValue& Value);
	void OnDamageActionStarted(const FInputActionValue& Value);
};
