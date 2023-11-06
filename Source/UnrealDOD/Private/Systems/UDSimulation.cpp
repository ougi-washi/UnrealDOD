// Copyright - Jed


#include "Systems/UDSimulation.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformProcess.h"

#define MAX_QUEUE_SIZE 50000
#define MAX_COMMANDS_PER_FRAME 5

int32 FUDSimulationState::RegisterActor(AActor* Actor)
{
	check(Actor);

	FUDActor ActorObj = {};
	ActorObj.Ptr = Actor;
	const int32 AddedIndex = Actors.AddUnique(ActorObj);
	if (!Actors.IsValidIndex(AddedIndex))
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
	Actors.RemoveAt(Index, 1, true);
}

FUDSimulationQueue& FUDSimulationState::GetActorMovementQueue(const int32& Index)
{
	check(Actors.IsValidIndex(Index) && Actors[Index]);
	return Actors[Index].MovementQueue;
}

TArray<int32> FUDSimulationState::UpdateLocations(const float& Delta)
{
	TArray<int32> ActorsToUpdate = {}; // This allows us to avoid updating actors that do not change

	for (int32 i = 0; i < Actors.Num(); i++)
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

	for (int32 i = 0; i < Actors.Num(); i++)
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

void FUDSimulationState::UpdateActorLocation(const int32& Index, const float& Delta)
{
	check(Actors.IsValidIndex(Index) && Actors[Index]);
	Actors[Index]->SetActorLocation(Locations[Index].Value);
}

void FUDSimulationState::UpdateActorRotation(const int32& Index, const float& Delta)
{
	check(Actors.IsValidIndex(Index) && Actors[Index]);
	Actors[Index]->SetActorRotation(Rotations[Index].Value);
}

void FUDSimulationState::UpdateActorsLocations(const TArray<int32>& Indices, const float& Delta)
{
	for (const int32& Index : Indices)
	{
		UpdateActorLocation(Index, Delta);
	}
}

void FUDSimulationState::UpdateActorsRotations(const TArray<int32>& Indices, const float& Delta)
{
	for (const int32& Index : Indices)
	{
		UpdateActorRotation(Index, Delta);
	}
}

void FUDSimulationQueue::ExecuteCommands()
{
	if (DoneExecuting())
	{
		return;
	}

	uint16 CurrentCommandIndex = 0;

	while (CurrentCommandIndex < Commands.Num() && CurrentCommandIndex < MAX_COMMANDS_PER_FRAME)
	{
		const bool bIsExecutionFrame = UKismetSystemLibrary::GetFrameCount() - Commands[CurrentCommandIndex].EnqueuedFrame >= Commands[CurrentCommandIndex].FrameDelay;
		if (Commands[CurrentCommandIndex].Lambda && bIsExecutionFrame)
		{
			Commands[CurrentCommandIndex].Lambda();
		}
		CurrentCommandIndex++;
	}

	FinishExecution();
}

void FUDSimulationQueue::Enqueue(const FUDSimulationCommand& Command)
{
	if (Commands.Num() < MAX_QUEUE_SIZE && !IsLocked())
	{
		Lock();
		Commands.Insert(Command, Commands.Num());
		Unlock();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FUDSimulationQueue::Enqueue - Reached max queue size"));
	}
}

void FUDSimulationQueue::Clear()
{
	Lock();
	Commands.Empty();
	Unlock();
}

FUDSimulation::FUDSimulation(UWorld* InWorld)
	: FUDSimulation()
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

	double PreviousFrameTime = FPlatformTime::Seconds();
	while (bIsRunning)
	{
		check(World);

		const double CurrentTime = FPlatformTime::Seconds();
		const double DeltaSeconds = CurrentTime - PreviousFrameTime;
		PreviousFrameTime = CurrentTime;

		const TArray<int32> LocationIndices = State.UpdateLocations(DeltaSeconds);
		const TArray<int32> RotationIndices = State.UpdateRotations(DeltaSeconds);
		TArray<int32> IndicesToUpdate = LocationIndices;
		IndicesToUpdate.Append(RotationIndices);
		
		for (const int32& IndexToUpdate : LocationIndices)
		{
			FUDSimulationQueue& CurrentQueue = State.GetActorMovementQueue(IndexToUpdate);

			if (CurrentQueue.DoneExecuting()) // Refresh if possible
			{
				CurrentQueue.Clear();
				EnqueueCommandToGameThread(CurrentQueue, FMath::RandHelper(10),
					[&, TempDelta = DeltaSeconds, TempIndexToUpdate = IndexToUpdate]()
					{
						State.UpdateActorLocation(TempIndexToUpdate, TempDelta);
						//State.UpdateActorRotation(TempIndexToUpdate, TempDelta);
					});
				CurrentQueue.WaitForExecution();
			}
		}

		// Maintain a consistent frame rate
		const double FrameTime = 1.0 / FramePerSecond;
		const double SleepTime = FrameTime - DeltaSeconds;
		if (SleepTime > 0.0)
		{
			FPlatformProcess::Sleep(SleepTime);
		}
	}

	return 0;
}

void FUDSimulation::Stop()
{
	bIsRunning = false;
}

void FUDSimulation::Tick_GameThread(const float& Delta)
{
	check(IsInGameThread());
	GeneralQueue.ExecuteCommands();
	for (int32 i = 0 ; i < State.Actors.Num(); i++)
	{
		State.GetActorMovementQueue(i).ExecuteCommands();
	}
}

void FUDSimulation::ReplicateIndex(const int32& Index, const bool& bSkipSource)
{
	
}

TArray<int32> FUDSimulation::GetDifferences(const FUDSimulationState& ClientState, const float& ErrorTolerence)
{
	return TArray<int32>();
}

void FUDSimulation::EnqueueCommandToGameThread(FUDSimulationQueue& Queue, const int64 FrameDelay, TFunction<void(void)> LambdaToAdd)
{
	FUDSimulationCommand Command = {};
	Command.Lambda = LambdaToAdd;
	Command.EnqueuedFrame = UKismetSystemLibrary::GetFrameCount();
	Command.FrameDelay = FrameDelay;
	Queue.Enqueue(Command);
}

void FUDSimulation::EnqueueGeneralCommandToGameThread(TFunction<void(void)> LambdaToAdd)
{
	EnqueueCommandToGameThread(GeneralQueue, 0, LambdaToAdd);
}
