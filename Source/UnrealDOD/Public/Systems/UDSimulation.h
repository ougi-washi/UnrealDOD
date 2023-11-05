// Copyright - Nexus Division

#pragma once

#include "CoreMinimal.h"

#define UD_DOD_TAG "DOD"

struct UNREALDOD_API FUDLocation
{
	FVector Value = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

struct UNREALDOD_API FUDMovement
{
	float Acceleration = 1.f;
	float Deceleration = 0.1f;
	float MaxSpeed = 100.f;
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
	TArray<AActor*>				ActorPtrs = {};
	TArray<FUDMovement>			Movements = {};
	TArray<FUDLocation>			Locations = {};
	TArray<FUDRotation>			Rotations = {};
	TArray<FUDMovementInput>	Inputs = {};
	TArray<int32>				IndicesToReplicate = {};

	int32 RegisterActor(AActor* Actor);
	void UnregisterActor(const int32& Index);

	TArray<int32> UpdateLocations(const float& Delta);	// Returns array of actors changed
	TArray<int32> UpdateRotations(const float& Delta);	// Returns array of actors changed

	void UpdateActorsLocations(const TArray<int32> Indices, const float& Delta);
	void UpdateActorsRotations(const TArray<int32> Indices, const float& Delta);
};

struct UNREALDOD_API FUDSimulationCommand
{
	TFunction<void(void)> Lambda = []() {};
};

struct UNREALDOD_API FUDSimulationQueue
{
	TArray<FUDSimulationCommand> Queue = {};

	void ExecuteCommands();
	void Enqueue(TFunction<void(void)> LambdaToExec);
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

	void EnqueueCommandToGameThread(TFunction<void(void)> LambdaToAdd);

protected:

	
private:

	FUDSimulation() {};

private:

	FUDSimulationState State = FUDSimulationState();
	FUDSimulationQueue Queue = FUDSimulationQueue();

	// Threading
	FRunnableThread* CurrentThread = nullptr;
	uint8 bIsRunning : 1;
	float FramePerSecond = 60.f;

	// Context
	UWorld* World = nullptr;
};