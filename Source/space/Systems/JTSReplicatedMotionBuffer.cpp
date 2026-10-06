#include "space/Systems/JTSReplicatedMotionBuffer.h"

void FJTSReplicatedMotionBuffer::Push(double ReceiveTime, const FVector& Location,
	const FQuat& Rotation, const FVector& Velocity)
{
	if (!FMath::IsFinite(ReceiveTime) || Location.ContainsNaN() || Rotation.ContainsNaN() || Velocity.ContainsNaN()) return;
	if (!Snapshots.IsEmpty())
	{
		const FSnapshot& Last = Snapshots.Last();
		if (ReceiveTime < Last.Time) return;
		// Teleports and relevancy re-entry must not leave a trailing path across the level.
		if (ReceiveTime - Last.Time > 0.5 || FVector::DistSquared(Location, Last.Location) > FMath::Square(500.0))
			Snapshots.Reset();
		else if (ReceiveTime == Last.Time)
			Snapshots.Pop(EAllowShrinking::No);
	}
	if (Snapshots.Num() == 8) Snapshots.RemoveAt(0, 1, EAllowShrinking::No);
	Snapshots.Add(FSnapshot{ReceiveTime, Location, Rotation.GetNormalized(), Velocity});
}

bool FJTSReplicatedMotionBuffer::Sample(double Time, float DelaySeconds, FVector& Location, FQuat& Rotation) const
{
	if (Snapshots.IsEmpty()) return false;
	const double RenderTime = Time - FMath::Max(0.0f, DelaySeconds);
	for (int32 I = 1; I < Snapshots.Num(); ++I)
	{
		const FSnapshot& A = Snapshots[I - 1];
		const FSnapshot& B = Snapshots[I];
		if (RenderTime > B.Time) continue;
		const double Duration = B.Time - A.Time;
		const float Alpha = Duration > UE_DOUBLE_SMALL_NUMBER
			? FMath::Clamp(float((RenderTime - A.Time) / Duration), 0.0f, 1.0f) : 1.0f;
		// Bounded Hermite tangents preserve velocity without overshooting stopped contacts.
		const double MaxTangent = FVector::Distance(A.Location, B.Location) * 3.0;
		Location = FMath::CubicInterp(A.Location, (A.Velocity * Duration).GetClampedToMaxSize(MaxTangent),
			B.Location, (B.Velocity * Duration).GetClampedToMaxSize(MaxTangent), Alpha);
		Rotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha).GetNormalized();
		return true;
	}
	const FSnapshot& End = RenderTime <= Snapshots[0].Time ? Snapshots[0] : Snapshots.Last();
	Location = End.Location;
	Rotation = End.Rotation;
	// Do not extrapolate an enemy through a wall during packet loss or after a collision.
	return true;
}
