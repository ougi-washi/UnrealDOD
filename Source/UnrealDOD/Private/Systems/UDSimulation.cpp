// Copyright - Nexus Division


#include "Systems/UDSimulation.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformProcess.h"


int32 FUDSimulationState::RegisterActor(AActor* Actor)
{
	check(Actor);

	const int32 AddedIndex = ActorPtrs.AddUnique(Actor);
	if (!ActorPtrs.IsValidIndex(AddedIndex))
	{
		return -1;
	}

	if (!Actor->ActorHasTag(UD_DOD_TAG))
	{
		Actor->Tags.Add(UD_DOD_TAG);
	}

	FUDLocation Location = {};
	Location.Value = Actor->GetActorLocation();
	const int32 PosIndex = Locations.Add(Location);
	ensureMsgf(PosIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add position properly because the indices seem to unmatch with the actor pointer."));

	FUDRotation Rotation = {};
	Rotation.Value = Actor->GetActorRotation();
	const int32 RotIndex = Rotations.Add(Rotation);
	ensureMsgf(RotIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add rotation properly because the indices seem to unmatch with the actor pointer."));

	return AddedIndex;
}

void FUDSimulationState::UnregisterActor(const int32& Index)
{
	Movements.RemoveAt(Index, 1, true);
	Locations.RemoveAt(Index, 1, true);
	Rotations.RemoveAt(Index, 1, true);
	Inputs.RemoveAt(Index, 1, true);
	ActorPtrs.RemoveAt(Index, 1, true);
}

void FUDSimulationState::UpdateLocations(const float& Delta)
{
	TArray<int32> ActorsToUpdate = {}; // This allows us to avoid updating actors that do not change

	for (int32 i = 0 ; i < ActorPtrs.Num() ; i++)
	{
		FUDLocation& Location = Locations[i];
		FUDMovement& Movement = Movements[i];
		FUDMovementInput& Input = Inputs[i];
		const FVector CachedLocation = Location.Value;

		FVector Acceleration = FVector::ZeroVector;

		Acceleration += Location.Input * Movement.Acceleration;

		if (Movement.Deceleration > 0.0f)
		{
			const FVector OppositeVelocity = -Location.Velocity;
			FVector DecelerationForce = OppositeVelocity.GetSafeNormal() * Movement.Deceleration;
			Acceleration += FMath::Min(Location.Velocity.Size(), DecelerationForce.Size()) * DecelerationForce;
		}

		Location.Velocity += Acceleration * Delta;
		Location.Velocity = Location.Velocity.GetClampedToMaxSize(Movement.MaxSpeed);

		Location.Value += Location.Velocity * Delta;

		if (CachedLocation != Location.Value)
		{
			ActorsToUpdate.Add(i);
		}
	}

	for (const int32& ActorToUpdate : ActorsToUpdate)
	{
		ActorPtrs[ActorToUpdate]->SetActorLocation(Locations[ActorToUpdate].Value);
	}
}

void FUDSimulationState::UpdateRotations(const float& Delta)
{
	TArray<int32> ActorsToUpdate = {}; // This allows us to avoid updating actors that do not change

	for (int32 i = 0; i < ActorPtrs.Num(); i++)
	{
		FUDRotation& Rotation = Rotations[i];
		FUDMovementInput& Input = Inputs[i];
		FRotator CachedRotation = Rotation.Value;

		Rotation.Value.Yaw += Input.Rotation.Y * Rotation.RotationSpeed;

		if (CachedRotation != Rotation.Value)
		{
			ActorsToUpdate.Add(i);
		}
	}

	for (const int32& ActorToUpdate : ActorsToUpdate)
	{
		ActorPtrs[ActorToUpdate]->SetActorRotation(Rotations[ActorToUpdate].Value);
	}
}

FUDSimulation::FUDSimulation()
{
	bIsRunning = false;
	CurrentThread = FRunnableThread::Create(this, TEXT("Unreal DOD Simulation"));
}

FUDSimulation::~FUDSimulation()
{
	if (CurrentThread)
	{
		CurrentThread->Kill();
		delete CurrentThread;
	}
}

void FUDSimulation::Start(UWorld* InWorld)
{
	if (InWorld)
	{
		World = InWorld;
		bIsRunning = true;
	}
}

void FUDSimulation::Stop()
{
	bIsRunning = false;
}

void FUDSimulation::ReplicateIndex(const int32& Index, const bool& bSkipSource)
{
	
}


TArray<int32> FUDSimulation::GetDifferences(const FUDSimulationState& ClientState, const float& ErrorTolerence)
{
	return TArray<int32>();
}

bool FUDSimulation::Init()
{
	return true;
}

uint32 FUDSimulation::Run()
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot run simulation with an invalid world, please set world"));
	}
	while (bIsRunning)
	{
		check(World);
		const float DeltaSeconds = UGameplayStatics::GetWorldDeltaSeconds(World);
		State.UpdateLocations(DeltaSeconds);
		State.UpdateRotations(DeltaSeconds);

		// Wait to the next frame time
		FPlatformProcess::ConditionalSleep([TempWorld = World, TempFps = FramePerSecond]() 
		{
			check(TempWorld);
			return UGameplayStatics::GetWorldDeltaSeconds(TempWorld) >= 1 / TempFps;
		});
	}
	return 0;
}
