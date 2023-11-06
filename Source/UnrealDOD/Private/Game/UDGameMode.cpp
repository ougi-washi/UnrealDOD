// Copyright - Jed


#include "Game/UDGameMode.h"
#include "Game/UDGameState.h"

AUDGameMode::AUDGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AUDGameState::StaticClass();
}