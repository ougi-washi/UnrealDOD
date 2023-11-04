// Copyright - Nexus Division


#include "Game/UDGameState.h"
#include "Engine/Level.h"


void AUDGameState::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	check(World);
	for (ULevel* Level : World->GetLevels())
	{
		check(Level);
		for (AActor* CurrentActor : Level->Actors)
		{
			if (CurrentActor->ActorHasTag(UD_DOD_TAG))
			{
				const int32 SimActorIndex = Simulation.RegisterActor(CurrentActor);
			}
		}
	}
	Simulation.Start(GetWorld());
}

void AUDGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	Simulation.Stop();
}
