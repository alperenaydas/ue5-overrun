// Fill out your copyright notice in the Description page of Project Settings.


#include "TopDownGameMode.h"

#include "OverrunHUD.h"
#include "TopDownPlayerController.h"
#include "TopDownPlayerState.h"

ATopDownGameMode::ATopDownGameMode()
{
	PlayerControllerClass = ATopDownPlayerController::StaticClass();
	HUDClass = AOverrunHUD::StaticClass();
	PlayerStateClass = ATopDownPlayerState::StaticClass();
}
