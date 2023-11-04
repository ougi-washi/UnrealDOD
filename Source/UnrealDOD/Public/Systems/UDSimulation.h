// Copyright - Nexus Division

#pragma once

#include "CoreMinimal.h"

struct UNREALDOD_API FUDLocation
{
	FVector Value = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Input = FVector::ZeroVector;
};

struct UNREALDOD_API FUDMovement
{
	float Acceleration = 1.f;
	float Deceleration = 1.f;
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

	void UpdateLocations(const float& Delta);
	void UpdateRotations(const float& Delta);
};

struct UNREALDOD_API FUDSimulation : public FRunnable
{

public:

	FUDSimulation();
	virtual ~FUDSimulation() override;

	void Start(UWorld* InWorld);
	void ReplicateIndex(const int32& Index, const bool& bSkipSource);
	TArray<int32> GetDifferences(const FUDSimulationState& ClientState, const float& ErrorTolerence); // Returns the list of indices that have to be corrected

protected:

	bool Init() override;
	uint32 Run() override;
	void Stop() override;

private:

	FUDSimulationState State = FUDSimulationState();

	// Threading
	FRunnableThread* CurrentThread = nullptr;
	uint8 bIsRunning : 1;
	float FramePerSecond = 60.f;

	// Context
	UWorld* World = nullptr;
};