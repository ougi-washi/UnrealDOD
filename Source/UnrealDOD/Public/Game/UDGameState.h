// Copyright - Nexus Division

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Systems/UDSimulation.h"
#include "UDGameState.generated.h"

/**
 * 
 */
UCLASS()
class UNREALDOD_API AUDGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:

	virtual void BeginPlay() override; 
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	FUDSimulation Simulation = FUDSimulation();
};
