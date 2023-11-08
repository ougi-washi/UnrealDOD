// Copyright - Jed


#include "Systems/UDSimulation.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define MAX_QUEUE_SIZE 50000
#define MAX_COMMANDS_PER_FRAME 5

void FUDSimulationQueue::ExecuteCommands()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimQueue_ExecuteCommands");
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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimQueue_EnqueueCommand");
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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimQueue_Clear");
	Lock();
	Commands.Empty();
	Unlock();
}

int32 FUDSimulationState::RegisterActor(AActor* Actor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_RegisterActor");
	check(Actor);
	
	WaitUntilUnlocked();
	bLocked = true;

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

	FUDRotation Rotation = {};
	Rotation.Value = Actor->GetActorRotation();
	const int32 RotIndex = Rotations.Add(Rotation);
	ensureMsgf(RotIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add rotation properly because the indices seem to unmatch with the actor pointer."));

	FUDMovement Movement = {};
	const int32 MovIndex = Movements.Add(Movement);
	ensureMsgf(MovIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add Movement properly because the indices seem to unmatch with the actor pointer."));

	FUDMovementInput Input = {};
	Input.Movement = FVector(1.f, 0.f, 0.f);
	const int32 InpIndex = Inputs.Add(Input);
	ensureMsgf(InpIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add Input properly because the indices seem to unmatch with the actor pointer."));

	FUDCollision Collision = {};
	const int32 CollIndex = Collisions.Add(Collision);
	ensureMsgf(CollIndex == AddedIndex, TEXT("FUDSimulationState::RegisterActor - Cannot add Collision properly because the indices seem to unmatch with the actor pointer."));

	bLocked = false;
	return AddedIndex;
}

void FUDSimulationState::UnregisterActor(const int32& Index)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UnregisterActor");
	WaitUntilUnlocked();
	bLocked = true;
	Actors.RemoveAt(Index, 1, true);
	Locations.RemoveAt(Index, 1, true);
	Rotations.RemoveAt(Index, 1, true);
	Movements.RemoveAt(Index, 1, true);
	Inputs.RemoveAt(Index, 1, true);
	Collisions.RemoveAt(Index, 1, true);
	bLocked = false;
}

FUDSimulationQueue& FUDSimulationState::GetActorMovementQueue(const int32& Index)
{
	check(Actors.IsValidIndex(Index) && Actors[Index]);
	return Actors[Index].MovementQueue;
}

TArray<int32> FUDSimulationState::UpdateLocations(const float& Delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateLocations");

	TArray<int32> ActorsToUpdate = {}; // This allows us to avoid updating actors that do not change

	for (int32 i = 0; i < Actors.Num(); i++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateLocation_Single" + i);
		if (!Locations.IsValidIndex(i) || !Movements.IsValidIndex(i) || !Inputs.IsValidIndex(i))
		{
			break;
		}
		FUDLocation& Location = Locations[i];
		FUDMovement& Movement = Movements[i];
		FUDMovementInput& Input = Inputs[i];
		const FVector CachedLocation = Location.Value;
		FVector Acceleration = FVector(0.f, 0.f, -Movement.Gravity);

		Acceleration += Input.Movement * Movement.Acceleration;

		if (Movement.Deceleration > 0.0f)
		{
			const FVector OppositeVelocity = -Location.Velocity;
			FVector DecelerationForce = OppositeVelocity.GetSafeNormal() * Movement.Deceleration;
			Acceleration += FMath::Min(Location.Velocity.Size(), DecelerationForce.Size()) * DecelerationForce;
		}

		Location.Velocity += Acceleration * Delta;
		Location.Velocity = Location.Velocity.GetClampedToMaxSize(Movement.MaxSpeed);

		if (Movement.bEnableCollision)
		{
			FUDActor& Actor = Actors[i];
			FUDCollision& Collision = Collisions[i];
			check(Actor.Get());

			UWorld* World = Actor->GetWorld();
			FVector NewLocation = CachedLocation + (Location.Velocity * Delta);
			FVector CollidedLocation = FVector::ZeroVector;
			FVector CollisionPos = FVector::ZeroVector;
			if (CheckCollision(CollidedLocation, CollisionPos, Actor.Get(), Collision, CachedLocation, NewLocation))
			{
				//Location.Value = Location.Velocity * Delta;
				// Ensure the character doesn't move below a certain height
				//Location.Value.Z = FMath::Max(Location.Value.Z, CollisionPos.Z /*+ Collision.Height*/);

			}
			else
			{
				Location.Value = CollidedLocation;
				// Ensure the character doesn't move below a certain height (e.g., character's capsule or feet height)
				//Location.Value.Z = FMath::Max(Location.Value.Z, Collision.Height);
			}
		}
		else
		{
			Location.Value = Location.Velocity * Delta;
			// Ensure the character doesn't move below a certain height
			//Location.Value.Z = FMath::Max(Location.Value.Z, Collision.Height);
		}

		if (CachedLocation != Location.Value)
		{
			ActorsToUpdate.Add(i);
		}
	}

	return ActorsToUpdate;
}

TArray<int32> FUDSimulationState::UpdateRotations(const float& Delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateRotations");
	TArray<int32> ActorsToUpdate = {}; // This allows us to avoid updating actors that do not change

	for (int32 i = 0; i < Actors.Num(); i++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateRotation_Single");
		if (!Rotations.IsValidIndex(i) || !Inputs.IsValidIndex(i))
		{
			break;
		}
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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateLocation");
	check(Actors.IsValidIndex(Index) && Actors[Index]);
	Actors[Index]->SetActorLocation(Locations[Index].Value);
}

void FUDSimulationState::UpdateActorRotation(const int32& Index, const float& Delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateRotation");
	check(Actors.IsValidIndex(Index) && Actors[Index]);
	Actors[Index]->SetActorRotation(Rotations[Index].Value);
}

void FUDSimulationState::UpdateActorsLocations(const TArray<int32>& Indices, const float& Delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateActorsLocations");
	for (const int32& Index : Indices)
	{
		UpdateActorLocation(Index, Delta);
	}
}

void FUDSimulationState::UpdateActorsRotations(const TArray<int32>& Indices, const float& Delta)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_UpdateActorsRotations");
	for (const int32& Index : Indices)
	{
		UpdateActorRotation(Index, Delta);
	}
}

bool FUDSimulationState::CheckCollision(FVector& OutPosition, FVector& HitPos, const AActor* Actor, const FUDCollision& Collision, const FVector& CurrentPosition, const FVector& TargetPosition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_CheckCollision");
	UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return false; // No valid world reference
	}

	FCollisionQueryParams CollisionParams = {};
	CollisionParams.AddIgnoredActor(Actor);
	TArray<FHitResult> HitResult = {};

	FVector SlideDirection = FVector::ZeroVector;
	OutPosition = TargetPosition;
	int32 Iterations = 0;
	while (World->SweepMultiByChannel(HitResult, CurrentPosition, OutPosition, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Collision.Size), CollisionParams))
	{
		// Find the normal of the steepest slope among the hits
		FVector BestSlopeNormal = FVector::UpVector; // Initialize with vertical direction
		float BestSlopeAngle = 0.0f;

		for (const FHitResult& Hit : HitResult)
		{
			FVector HitNormal = Hit.ImpactNormal;
			float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(HitNormal.Z));
			if (SlopeAngle > BestSlopeAngle)
			{
				BestSlopeAngle = SlopeAngle;
				BestSlopeNormal = HitNormal;
				HitPos = Hit.Location;
			}
		}

		// Check the slope of the steepest hit
		if (BestSlopeAngle <= Collision.AcceptableSlope)
		{
			// The steepest slope is within an acceptable range, allow movement
			return true;
		}
		else
		{
			// The steepest slope is too steep, so handle the collision by sliding along the slope
			SlideDirection = FVector::CrossProduct(BestSlopeNormal, FVector::UpVector).GetSafeNormal();
			float SlideDistance = Collision.Size * FMath::Cos(FMath::DegreesToRadians(Collision.AcceptableSlope));
			FVector NewTargetLocationFound = OutPosition - SlideDirection * SlideDistance;
			
			if (NewTargetLocationFound.Equals(OutPosition, UE_KINDA_SMALL_NUMBER))
			{
				// The sliding operation did not result in a change; exit the loop
				break;
			}

			if (FVector::Distance(TargetPosition, NewTargetLocationFound) <= Collision.AcceptableDistance)
			{
				OutPosition = NewTargetLocationFound;
			}
			else
			{
				return true;
			}
		}

		// Clear the HitResult array for the next iteration
		HitResult.Empty();

		Iterations++;
		if (Iterations >= Collision.MaxSlopeIteration)
		{
			return false; // Reached the maximum number of iterations
		}
	}

	return false; // No collisions
}

void FUDSimulationState::WaitUntilUnlocked()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SimState_WaitUntilUnlocked");
	FPlatformProcess::ConditionalSleep([&]() {return !bLocked; }, .001f);
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
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Sim_MainLoop");
		check(World);

		const double CurrentTime = FPlatformTime::Seconds();
		const double DeltaSeconds = CurrentTime - PreviousFrameTime;
		PreviousFrameTime = CurrentTime;

		State.WaitUntilUnlocked();
		State.bLocked = true;
		const TArray<int32> LocationIndices = State.UpdateLocations(DeltaSeconds);
		const TArray<int32> RotationIndices = State.UpdateRotations(DeltaSeconds);
		TArray<int32> IndicesToUpdate = LocationIndices;
		for (const int32& RotationIndex : RotationIndices)
		{
			IndicesToUpdate.AddUnique(RotationIndex);
		}
		
		for (const int32& IndexToUpdate : IndicesToUpdate)
		{
			FUDSimulationQueue& CurrentQueue = State.GetActorMovementQueue(IndexToUpdate);

			if (CurrentQueue.DoneExecuting()) // Refresh if possible
			{
				CurrentQueue.Clear();
				EnqueueCommandToGameThread(CurrentQueue, FMath::RandHelper(1),
					[&, TempDelta = DeltaSeconds, TempIndexToUpdate = IndexToUpdate]()
					{
						State.UpdateActorLocation(TempIndexToUpdate, TempDelta);
						State.UpdateActorRotation(TempIndexToUpdate, TempDelta);
					});
				CurrentQueue.WaitForExecution();
			}
		}
		State.bLocked = false;

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
