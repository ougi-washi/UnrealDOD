// Copyright - Jed

#pragma once

#include "CoreMinimal.h"

#define UD_DOD_TAG "DOD"

struct UNREALDOD_API FUDSimulationCommand
{
	TFunction<void(void)> Lambda = []() {};
	int64 FrameDelay = 0;
	int64 EnqueuedFrame = 0;
};

struct UNREALDOD_API FUDSimulationQueue
{
	TArray<FUDSimulationCommand> Commands = {};
	
	FUDSimulationQueue() : bFinishedExecution(false), bIsLocked(false) {};

	void ExecuteCommands();
	void Enqueue(const FUDSimulationCommand& Command);
	void Clear();

	bool IsLocked() const { return bIsLocked; };					// Simulation thread
	void Lock() { check(!IsInGameThread()); bIsLocked = true; };	// Simulation thread
	void Unlock() { check(!IsInGameThread()); bIsLocked = false; };	// Simulation thread

	bool DoneExecuting() const { return bFinishedExecution; };		// Game thread
	void FinishExecution() { bFinishedExecution = true; };			// Game thread
	void WaitForExecution() { bFinishedExecution = false; };		// Game thread

protected:

	uint8 bFinishedExecution : 1;
	uint8 bIsLocked : 1; // This makes sure no concurrency issues happen with the queue
};

struct UNREALDOD_API FUDActor
{
	AActor* Ptr = nullptr;
	FUDSimulationQueue MovementQueue = FUDSimulationQueue();

	FORCEINLINE AActor* Get() { return Ptr; };
	FORCEINLINE AActor* operator->() { return Ptr; };
	FORCEINLINE operator const bool() const { return (bool)Ptr; };
	FORCEINLINE const bool operator==(const FUDActor& Other) const { return this->Ptr == Other.Ptr; };
};

struct UNREALDOD_API FUDLocation
{
	FVector Value = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

struct UNREALDOD_API FUDMovement
{
	float Acceleration = 10.f;
	float Deceleration = 0.1f;
	float MaxSpeed = 1000.f;
	float Gravity = 9.8f; // Make it a vector if direction is needed
	uint8 bEnableCollision : 1;

	FUDMovement() : bEnableCollision(true) {};
}; 

struct UNREALDOD_API FUDCollision
{
	float Size = 50.f;
	float Height = 0.f;
	float AcceptableSlope = 4.f;
	uint8 MaxSlopeIteration = 3;
};

struct UNREALDOD_API FUDRotation
{
	FRotator Value = FRotator::ZeroRotator;
	float RotationSpeed = 1.f;
};

struct UNREALDOD_API FUDMovementInput
{
	FVector Movement = FVector::ZeroVector;
	FVector Rotation = FVector::ZeroVector;
};

struct UNREALDOD_API FUDSimulationState
{
	TArray<FUDLocation>			Locations	= {};
	TArray<FUDRotation>			Rotations	= {};
	TArray<FUDMovement>			Movements	= {};
	TArray<FUDMovementInput>	Inputs		= {};
	TArray<FUDCollision>		Collisions	= {};
	TArray<FUDActor>			Actors = {}; // put this at the end for a better data layout
	TArray<int32>				IndicesToReplicate = {};

	int32 RegisterActor(AActor* Actor);
	void UnregisterActor(const int32& Index);
	FUDSimulationQueue& GetActorMovementQueue(const int32& Index);

	TArray<int32> UpdateLocations(const float& Delta);	// Returns array of actors changed
	TArray<int32> UpdateRotations(const float& Delta);	// Returns array of actors changed
	void UpdateActorLocation(const int32& Index, const float& Delta);
	void UpdateActorRotation(const int32& Index, const float& Delta);
	void UpdateActorsLocations(const TArray<int32>& Indices, const float& Delta);
	void UpdateActorsRotations(const TArray<int32>& Indices, const float& Delta);

	bool CheckCollision(const AActor* Actor, const FUDCollision& Collision, const FVector& CurrentPosition, FVector& TargetPosition);

	void WaitUntilUnlocked();

	uint8 bLocked : 1;
	FUDSimulationState() : bLocked(false) {};
};


struct UNREALDOD_API FUDSimulation : public FRunnable
{

public:

	FUDSimulation(UWorld* InWorld);
	virtual ~FUDSimulation() override;

	// Thread
	bool Init() override;
	uint32 Run() override;
	void Stop() override;

	// Simulation
	void Tick_GameThread(const float& Delta);
	
	FORCEINLINE int32 RegisterActor(AActor* Actor) { return State.RegisterActor(Actor); };
	FORCEINLINE void UnregisterActor(const int32& Index) { return State.UnregisterActor(Index); };

	void ReplicateIndex(const int32& Index, const bool& bSkipSource);
	TArray<int32> GetDifferences(const FUDSimulationState& ClientState, const float& ErrorTolerence); // Returns the list of indices that have to be corrected

	void EnqueueCommandToGameThread(FUDSimulationQueue& Queue, const int64 FrameDelay, TFunction<void(void)> LambdaToAdd);
	void EnqueueGeneralCommandToGameThread(TFunction<void(void)> LambdaToAdd);
	
private:

	FUDSimulation() : bIsRunning(false) {};

private:

	FUDSimulationState State = FUDSimulationState();
	FUDSimulationQueue GeneralQueue = FUDSimulationQueue();

	// Threading
	FRunnableThread* CurrentThread = nullptr;
	uint8 bIsRunning : 1;
	double FramePerSecond = 30.;

	// Context
	UWorld* World = nullptr;
};