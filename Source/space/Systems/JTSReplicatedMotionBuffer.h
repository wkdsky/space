#pragma once

#include "CoreMinimal.h"

/** Small presentation buffer for server-owned actors; never runs client gameplay or predicts collisions. */
class SPACE_API FJTSReplicatedMotionBuffer
{
public:
	void Push(double ReceiveTime, const FVector& Location, const FQuat& Rotation, const FVector& Velocity);
	bool Sample(double Time, float DelaySeconds, FVector& Location, FQuat& Rotation) const;
	void Reset() { Snapshots.Reset(); }

private:
	struct FSnapshot
	{
		double Time;
		FVector Location;
		FQuat Rotation;
		FVector Velocity;
	};
	TArray<FSnapshot, TInlineAllocator<8>> Snapshots;
};
