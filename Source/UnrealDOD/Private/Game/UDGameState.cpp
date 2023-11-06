// Copyright - Jed


#include "Game/UDGameState.h"
#include "Engine/Level.h"


AUDGameState::AUDGameState(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AUDGameState::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	check(World);

	Simulation = new FUDSimulation(GetWorld());
	if (Simulation)
	{
		for (ULevel* Level : World->GetLevels())
		{
			check(Level);
			for (AActor* CurrentActor : Level->Actors)
			{
				if (IsValid(CurrentActor) && CurrentActor->ActorHasTag(UD_DOD_TAG))
				{
					const int32 SimActorIndex = Simulation->RegisterActor(CurrentActor);
				}
			}
		}
	}
}

void AUDGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Simulation)
	{
		Simulation->Tick_GameThread(DeltaSeconds);
	}
}

void AUDGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (Simulation)
	{
		Simulation->Stop();
		delete Simulation;
	}
}
