// Copyright - Nexus Division


#include "Systems/UDSimulation.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformProcess.h"

#define MAX_QUEUE_SIZE 5000

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

	FUDMovement Movement = {};
	const int32 MovIndex = Movements.Add(Movement);
	ensureMsgf(MovIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add Movement properly because the indices seem to unmatch with the actor pointer."));

	FUDMovementInput Input = {};
	Input.Movement = FVector(1.f, 0.f, 0.f);
	const int32 InpIndex = Inputs.Add(Input);
	ensureMsgf(InpIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add Input properly because the indices seem to unmatch with the actor pointer."));

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

TArray<int32> FUDSimulationState::UpdateLocations(const float& Delta)
{
	TArray<int32> ActorsToUpdate = {}; // This allows us to avoid updating actors that do not change

	for (int32 i = 0; i < ActorPtrs.Num(); i++)
	{
		FUDLocation& Location = Locations[i];
		FUDMovement& Movement = Movements[i];
		FUDMovementInput& Input = Inputs[i];
		const FVector CachedLocation = Location.Value;

		FVector Acceleration = FVector::ZeroVector;

		Acceleration += Input.Movement * Movement.Acceleration;

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

	return ActorsToUpdate;
}

TArray<int32> FUDSimulationState::UpdateRotations(const float& Delta)
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

	return ActorsToUpdate;
}

void FUDSimulationState::UpdateActorsLocations(const TArray<int32> Indices, const float& Delta)
{
	for (const int32& Index : Indices)
	{
		ActorPtrs[Index]->SetActorLocation(Locations[Index].Value);
	}
}

void FUDSimulationState::UpdateActorsRotations(const TArray<int32> Indices, const float& Delta)
{
	for (const int32& Index : Indices)
	{
		ActorPtrs[Index]->SetActorRotation(Rotations[Index].Value);
	}
}

void FUDSimulationQueue::ExecuteCommands()
{
	uint16 CurrentQueueIndex = MAX_QUEUE_SIZE;

	while (Queue.Num() > 0)
	{
		if (Queue[0].Lambda)
		{
			Queue[0].Lambda();
		}
		Queue.RemoveAt(0, 0, true);

		// Avoid infinite loop
		CurrentQueueIndex--;
		if (CurrentQueueIndex <= 0)
		{
			return;
		}
	}
}

void FUDSimulationQueue::Enqueue(TFunction<void(void)> LambdaToExec)
{
	if (Queue.Num() < MAX_QUEUE_SIZE)
	{
		Queue.Add({ LambdaToExec });
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FUDSimulationQueue::Enqueue - Reached max queue size"));
	}
}

FUDSimulation::FUDSimulation(UWorld* InWorld)
{
	if (InWorld)
	{
		World = InWorld;
		bIsRunning = true;
		CurrentThread = FRunnableThread::Create(this, TEXT("Unreal DOD Simulation"));
	}
}

FUDSimulation::~FUDSimulation()
{
	if (CurrentThread)
	{
		CurrentThread->Kill();
		delete CurrentThread;
	}
}

void FUDSimulation::Tick_GameThread(const float& Delta)
{
	check(IsInGameThread());
	Queue.ExecuteCommands();
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

void FUDSimulation::EnqueueCommandToGameThread(TFunction<void(void)> LambdaToAdd)
{
	Queue.Enqueue(LambdaToAdd);
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

		const TArray<int32> LocationIndices =  State.UpdateLocations(DeltaSeconds);
		const TArray<int32> RotationIndices = State.UpdateRotations(DeltaSeconds);

		EnqueueCommandToGameThread([&, TempDelta = DeltaSeconds, TempLocationIndices = LocationIndices, TempRotationIndices = RotationIndices]()
		{
			State.UpdateActorsLocations(TempLocationIndices, TempDelta);
			State.UpdateActorsRotations(TempRotationIndices, TempDelta);
		});

		// Wait to the next frame time
		FPlatformProcess::Sleep(1.f / FramePerSecond);

		//FPlatformProcess::ConditionalSleep([TempWorld = World, TempFps = FramePerSecond]() 
		//{
		//	check(TempWorld);
		//	return UGameplayStatics::GetWorldDeltaSeconds(TempWorld) >= 1 / TempFps;
		//});
	}

	return 0;
}
