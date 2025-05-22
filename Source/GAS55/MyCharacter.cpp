// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Interface/CombatInterface.h"
#include "Kismet/GameplayStatics.h"

AMyCharacter::AMyCharacter(): CurrentAttackTarget(nullptr), Pull_Group_InitialWorldCentroid(),
                              Pull_Group_TargetWorldCentroid()
{
	PrimaryActorTick.bCanEverTick = true;

	bIsGroupPullActive = false;
	bIsDrawDebug = false;
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetMesh() != nullptr)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.AddDynamic(this, &AMyCharacter::OnMontageEndedEvent);
		}
	}
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

TArray<AMyCharacter*> AMyCharacter::FindCharactersInYawAngle(const float MaxYawAngleDegrees, const float MaxRange,
                                                             const bool bInDrawDebug)
{
	TArray<AMyCharacter*> CharactersInAngleAndRange;
	TArray<AActor*> AllFoundActors;

	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMyCharacter::StaticClass(), AllFoundActors);

	const FVector SelfLocation = GetActorLocation();
	const FVector SelfForwardVector = GetActorForwardVector();
	const float MaxRangeSquared = FMath::Square(MaxRange);

	FVector SelfForwardHorizontal = SelfForwardVector;
	SelfForwardHorizontal.Z = 0.0f;
	if (!SelfForwardHorizontal.Normalize())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "[%s::FindCharactersInYawAngle] Could not normalize SelfForwardHorizontal, character might be looking straight up/down."
		       ),
		       *GetName());
		return CharactersInAngleAndRange;
	}

	const float AngleThresholdCosine = FMath::Cos(FMath::DegreesToRadians(MaxYawAngleDegrees));
	constexpr float DebugDrawTime = 10.0f;

	if (bInDrawDebug)
	{
		const FVector LeftConeEdgeDirection = SelfForwardHorizontal.RotateAngleAxis(
			-MaxYawAngleDegrees, FVector::UpVector);
		const FVector RightConeEdgeDirection = SelfForwardHorizontal.RotateAngleAxis(
			MaxYawAngleDegrees, FVector::UpVector);

		DrawDebugLine(GetWorld(), SelfLocation, SelfLocation + LeftConeEdgeDirection * MaxRange, FColor::Yellow, false,
		              DebugDrawTime, 0, 2.0f);
		DrawDebugLine(GetWorld(), SelfLocation, SelfLocation + RightConeEdgeDirection * MaxRange, FColor::Yellow, false,
		              DebugDrawTime, 0, 2.0f);
		DrawDebugLine(GetWorld(), SelfLocation, SelfLocation + SelfForwardHorizontal * MaxRange, FColor::Orange, false,
		              DebugDrawTime, 0, 2.0f);
	}

	for (AActor* Actor : AllFoundActors)
	{
		AMyCharacter* OtherCharacter = Cast<AMyCharacter>(Actor);

		if (OtherCharacter && OtherCharacter != this)
		{
			const FVector OtherLocation = OtherCharacter->GetActorLocation();
			const FVector DirectionToOther = OtherLocation - SelfLocation;

			if (DirectionToOther.SizeSquared() > MaxRangeSquared)
			{
				if (bInDrawDebug)
				{
					DrawDebugLine(GetWorld(), SelfLocation, OtherLocation, FColor::Blue, false, DebugDrawTime, 0, 1.0f);
				}
				continue;
			}

			FVector DirectionToOtherHorizontal = DirectionToOther;
			DirectionToOtherHorizontal.Z = 0.0f;
			if (!DirectionToOtherHorizontal.Normalize())
			{
				if (bInDrawDebug)
				{
					DrawDebugLine(GetWorld(), SelfLocation, OtherLocation, FColor::White, false, DebugDrawTime, 0,
					              1.0f);
				}
				continue;
			}

			const float DotProductHorizontal = FVector::DotProduct(SelfForwardHorizontal, DirectionToOtherHorizontal);
			if (DotProductHorizontal > AngleThresholdCosine)
			{
				CharactersInAngleAndRange.Add(OtherCharacter);
				if (bInDrawDebug)
				{
					DrawDebugLine(GetWorld(), SelfLocation, OtherLocation, FColor::Green, false, DebugDrawTime, 0,
					              2.5f);
					DrawDebugSphere(GetWorld(), OtherLocation, 50.f, 12, FColor::Green, false, DebugDrawTime, 0, 1.5f);
				}
			}
			else
			{
				if (bInDrawDebug)
				{
					DrawDebugLine(GetWorld(), SelfLocation, OtherLocation, FColor::Red, false, DebugDrawTime, 0, 1.0f);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("%s::FindCharactersInYawAngle found %d characters in range."), *GetName(),
	       CharactersInAngleAndRange.Num());
	return CharactersInAngleAndRange;
}

AMyCharacter* AMyCharacter::SelectBestAttackTargetFromList(const TArray<AMyCharacter*>& PotentialTargets,
                                                           const bool bInDrawDebug)
{
	UE_LOG(LogTemp, Log, TEXT("SelectBestAttackTargetFromList: Called with %d potential targets."),
	       PotentialTargets.Num());

	if (PotentialTargets.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("SelectBestAttackTargetFromList: PotentialTargets is empty. Returning nullptr."));
		return nullptr;
	}

	AMyCharacter* SelectedTarget = nullptr;
	float MinDistanceSquaredActual = TNumericLimits<float>::Max();

	int ValidPotentialTargetsCount = 0;
	for (const AMyCharacter* CurrentTarget : PotentialTargets)
	{
		if (!CurrentTarget)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "SelectBestAttackTargetFromList: Found a nullptr target in PotentialTargets during min distance calculation."
			       ));
			continue;
		}
		ValidPotentialTargetsCount++;
		const float DistSq = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());
		if (DistSq < MinDistanceSquaredActual)
		{
			MinDistanceSquaredActual = DistSq;
		}
	}

	if (ValidPotentialTargetsCount == 0)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "SelectBestAttackTargetFromList: No valid targets in PotentialTargets after checking for nullptr. Returning nullptr."
		       ));
		return nullptr;
	}
	if (MinDistanceSquaredActual == TNumericLimits<float>::Max())
	{
		UE_LOG(LogTemp, Error,
		       TEXT(
			       "SelectBestAttackTargetFromList: MinDistanceSquaredActual was not updated. Problem with distance calculation or targets. Defaulting to first valid potential target if any."
		       ));
		for (AMyCharacter* PotTarget : PotentialTargets) { if (PotTarget) return PotTarget; }
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("SelectBestAttackTargetFromList: MinDistanceSquaredActual = %f"),
	       MinDistanceSquaredActual);

	TArray<AMyCharacter*> ClosestTargetsConsidered;
	constexpr float DistanceToleranceSquared = FMath::Square(50.0f);

	for (AMyCharacter* CurrentTarget : PotentialTargets)
	{
		if (!CurrentTarget) continue;
		const float DistSq = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());
		if (FMath::IsNearlyEqual(DistSq, MinDistanceSquaredActual, DistanceToleranceSquared))
		{
			ClosestTargetsConsidered.Add(CurrentTarget);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SelectBestAttackTargetFromList: ClosestTargetsConsidered.Num() = %d"),
	       ClosestTargetsConsidered.Num());

	if (ClosestTargetsConsidered.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
		       TEXT(
			       "SelectBestAttackTargetFromList: ClosestTargetsConsidered IS EMPTY! This indicates a logic flaw or bad input. PotentialTargets had %d valid items."
		       ), ValidPotentialTargetsCount);
		return nullptr;
	}

	if (ClosestTargetsConsidered.Num() == 1)
	{
		SelectedTarget = ClosestTargetsConsidered[0];
		UE_LOG(LogTemp, Log, TEXT("SelectBestAttackTargetFromList: Only one closest target: %s"),
		       *SelectedTarget->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log,
		       TEXT(
			       "SelectBestAttackTargetFromList: Multiple (%d) equally close targets. Performing tie-breaker by angle."
		       ), ClosestTargetsConsidered.Num());
		FVector SelfForwardHorizontal = GetActorForwardVector();
		SelfForwardHorizontal.Z = 0.0f;

		if (!SelfForwardHorizontal.Normalize())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "SelectBestAttackTargetFromList: SelfForwardHorizontal FAILED to normalize (player looking straight up/down). Selecting first target from ClosestTargetsConsidered as fallback."
			       ));
			SelectedTarget = ClosestTargetsConsidered[0];
		}
		else
		{
			float MaxDotProduct = -2.0f;
			for (AMyCharacter* CloseTarget : ClosestTargetsConsidered)
			{
				if (!CloseTarget) continue;
				FVector DirToTargetHorizontal = CloseTarget->GetActorLocation() - GetActorLocation();
				DirToTargetHorizontal.Z = 0.0f;

				if (DirToTargetHorizontal.Normalize())
				{
					const float CurrentDot = FVector::DotProduct(SelfForwardHorizontal, DirToTargetHorizontal);
					if (CurrentDot > MaxDotProduct)
					{
						MaxDotProduct = CurrentDot;
						SelectedTarget = CloseTarget;
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning,
					       TEXT(
						       "SelectBestAttackTargetFromList: DirToTargetHorizontal for '%s' FAILED to normalize (target directly above/below). Skipping for angle sort."
					       ), *CloseTarget->GetName());
				}
			}

			if (!SelectedTarget && !ClosestTargetsConsidered.IsEmpty())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT(
					       "SelectBestAttackTargetFromList: Tie-breaker finished but no target selected (all targets directly above/below or other issue). Defaulting to first from ClosestTargetsConsidered."
				       ));
				SelectedTarget = ClosestTargetsConsidered[0];
			}
		}
	}

	if (bInDrawDebug && SelectedTarget)
	{
		DrawDebugLine(GetWorld(), GetActorLocation(), SelectedTarget->GetActorLocation(), FColor::Magenta, false, 2.f,
		              0, 5.f);
		DrawDebugSphere(GetWorld(), SelectedTarget->GetActorLocation(), 75.f, 12, FColor::Magenta, false, 2.f, 0, 3.f);
		UE_LOG(LogTemp, Log, TEXT("SelectBestAttackTargetFromList: FINAL SelectedTarget = %s (Debug drawing)"),
		       *SelectedTarget->GetName());
	}
	else if (bInDrawDebug && !SelectedTarget)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SelectBestAttackTargetFromList: bDrawDebugTargets is true, but no target was selected to draw."));
	}

	if (!SelectedTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("SelectBestAttackTargetFromList: Returning nullptr at the very end."));
	}

	return SelectedTarget;
}

void AMyCharacter::PerformAttack(const FName& AttackCode)
{
	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (AnimInst && AnimInst->IsAnyMontagePlaying())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s PerformAttack: Cannot attack, a montage is already playing. Code: %s"),
		       *GetName(), *AttackCode.ToString());
		return;
	}

	if (GetMovementComponent()->IsFalling())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s PerformAttack: Cannot attack while falling. Code: %s"), *GetName(),
		       *AttackCode.ToString());
		return;
	}

	if (!GetMovementComponent()->IsMovingOnGround())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s PerformAttack: Cannot attack when not moving on ground. Code: %s"),
		       *GetName(), *AttackCode.ToString());
		return;
	}

	const FCombatAnimationPair* AnimPair = CombatAnimationDatabase.Find(AttackCode);
	if (!AnimPair || !AnimPair->AttackerMontage)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("%s PerformAttack: AttackCode '%s' or its AttackerMontage not found in CombatAnimationDatabase."),
		       *GetName(), *AttackCode.ToString());
		return;
	}

	const TArray<AMyCharacter*> PotentialTargets = FindCharactersInYawAngle(
		AttackDetectionAngle, AttackDetectionRange, bIsDrawDebug);

	auto* BestTarget = SelectBestAttackTargetFromList(PotentialTargets, bIsDrawDebug);

	if (BestTarget == nullptr)
	{
		return;
	}

	CurrentAttackTarget = BestTarget;

	const FVector MyLocation = GetActorLocation();
	const FVector TargetLocation = CurrentAttackTarget->GetActorLocation();

	const FVector DirectionToTarget = (TargetLocation - MyLocation).GetSafeNormal();
	const FRotator LookAtRotationForSelf = DirectionToTarget.Rotation();

	FRotator CurrentSelfActorRotation = GetActorRotation();
	CurrentSelfActorRotation.Yaw = LookAtRotationForSelf.Yaw;
	SetActorRotation(CurrentSelfActorRotation);
	if (AController* MyController = GetController())
	{
		FRotator CurrentControllerRotation = MyController->GetControlRotation();
		CurrentControllerRotation.Yaw = LookAtRotationForSelf.Yaw;
		CurrentControllerRotation.Pitch = LookAtRotationForSelf.Pitch;
		MyController->SetControlRotation(CurrentControllerRotation);
	}
	const FVector DirectionFromTargetToSelf = (MyLocation - TargetLocation).GetSafeNormal();
	FRotator LookAtRotationForTarget = DirectionFromTargetToSelf.Rotation();

	FRotator CurrentTargetActorRotation = CurrentAttackTarget->GetActorRotation();
	CurrentTargetActorRotation.Yaw = LookAtRotationForTarget.Yaw;
	CurrentAttackTarget->SetActorRotation(CurrentTargetActorRotation);

	CurrentExecutingComboName = AttackCode;

	if (AnimInst)
	{
		AnimInst->Montage_Play(AnimPair->AttackerMontage);
		if (!(AnimPair->VictimRelativeTransformToAttacker.Equals(FTransform::Identity)))
		{
			// OnHandleApplyVictimRelativeTransform(AnimPair->VictimRelativeTransformToAttacker);
			ApplyVictimRelativeTransform(CurrentAttackTarget, AnimPair->VictimRelativeTransformToAttacker);
		}
		if (CurrentAttackTarget)
		{
			UE_LOG(LogTemp, Log, TEXT("%s performing '%s'. Target: %s"), *GetName(),
			       *CurrentExecutingComboName.ToString(), *CurrentAttackTarget->GetName());

			UCapsuleComponent* SelfCapsule = GetCapsuleComponent();
			UCapsuleComponent* TargetCapsule = CurrentAttackTarget->GetCapsuleComponent();

			if (SelfCapsule && TargetCapsule)
			{
				SelfCapsule->IgnoreActorWhenMoving(CurrentAttackTarget, true);
				TargetCapsule->IgnoreActorWhenMoving(this, true);

				UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Set %s and %s to ignore each other's movement collision."),
				       *GetName(), __FUNCTION__, *GetName(), *CurrentAttackTarget->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("%s performing '%s' (no specific target)."), *GetName(),
			       *CurrentExecutingComboName.ToString());
		}
	}
}

void AMyCharacter::ProcessAttackHit()
{
	if (!CurrentAttackTarget)
	{
		UE_LOG(LogTemp, Log, TEXT("%s ProcessAttackHit: No CurrentAttackTarget for combo '%s'."), *GetName(),
		       *CurrentExecutingComboName.ToString());
		return;
	}
	if (CurrentExecutingComboName.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("%s ProcessAttackHit: CurrentExecutingComboName is None! Cannot process hit."),
		       *GetName());
		return;
	}

	const FCombatAnimationPair* AnimPair = CombatAnimationDatabase.Find(CurrentExecutingComboName);
	if (!AnimPair)
	{
		UE_LOG(LogTemp, Error, TEXT("%s ProcessAttackHit: Combo data for '%s' not found!"), *GetName(),
		       *CurrentExecutingComboName.ToString());
		return;
	}
	if (!AnimPair->VictimReactionMontage)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "%s ProcessAttackHit: VictimReactionMontage for combo '%s' is NULL. Target will not play reaction montage."
		       ), *GetName(), *CurrentExecutingComboName.ToString());
	}

	if (CurrentAttackTarget->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
	{
		Execute_OnHitReceived(CurrentAttackTarget, this, AnimPair->VictimReactionMontage);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("%s ProcessAttackHit: CurrentAttackTarget %s does not implement ICombatInterface."), *GetName(),
		       *CurrentAttackTarget->GetName());
	}
}

void AMyCharacter::ApplyVictimRelativeTransform(AMyCharacter* Victim, const FTransform& RelativeTransform) const
{
	if (!Victim || Victim == this) return;
	const FTransform AttackerWorldTransform = GetActorTransform();
	const FTransform VictimTargetWorldTransform = RelativeTransform * AttackerWorldTransform;
	// Victim->SetActorLocationAndRotation(VictimTargetWorldTransform.GetLocation(),
	// 									VictimTargetWorldTransform.GetRotation(),
	// 									false, nullptr, ETeleportType::TeleportPhysics);
	const FTransform TargetTransform = FTransform(VictimTargetWorldTransform.GetRotation(),
	                                              VictimTargetWorldTransform.GetLocation(),
	                                              FVector::ZeroVector);
	Victim->TargetRelativeTransform = TargetTransform;

	UE_LOG(LogTemp, Log, TEXT("Teleported Victim %s to relative transform for combo %s"), *Victim->GetName(),
	       *CurrentExecutingComboName.ToString());
}

void AMyCharacter::HandleApplyVictimRelativeTransform()
{
	const FTransform& RelativeTransform = CurrentAttackTarget->TargetRelativeTransform;
	if (CurrentAttackTarget)
	{
		// CurrentAttackTarget->SetActorLocationAndRotation(RelativeTransform.GetLocation(), RelativeTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		// CurrentAttackTarget->GetMesh()->SetWorldLocationAndRotation(RelativeTransform.GetLocation(), RelativeTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		CurrentAttackTarget->OnHandleApplyVictimRelativeTransform(RelativeTransform);
	}

	OnHandleStartPullObject();
}

void AMyCharacter::OnHitReceived_Implementation(AActor* Attacker, UAnimMontage* VictimReactionMontageToPlay)
{
	UE_LOG(LogTemp, Log, TEXT("%s OnHitReceived: Attacked by %s. Will play montage: %s"),
	       *GetName(),
	       Attacker ? *Attacker->GetName() : TEXT("Unknown Attacker"),
	       VictimReactionMontageToPlay ? *VictimReactionMontageToPlay->GetName() : TEXT("NULL"));

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && VictimReactionMontageToPlay)
	{
		if (!CurrentExecutingComboName.IsNone())
		{
			const FCombatAnimationPair* MyCurrentAttackPair = CombatAnimationDatabase.Find(CurrentExecutingComboName);
			if (MyCurrentAttackPair && MyCurrentAttackPair->AttackerMontage && AnimInstance->Montage_IsPlaying(
				MyCurrentAttackPair->AttackerMontage))
			{
				AnimInstance->Montage_Stop(0.15f, MyCurrentAttackPair->AttackerMontage);
				UE_LOG(LogTemp, Log,
				       TEXT("%s OnHitReceived: Own attack montage for combo '%s' stopped to play hit reaction."),
				       *GetName(), *CurrentExecutingComboName.ToString());
			}
		}
		else if (AnimInstance->IsAnyMontagePlaying() && AnimInstance->GetCurrentActiveMontage() !=
			VictimReactionMontageToPlay)
		{
			AnimInstance->Montage_Stop(0.15f);
			UE_LOG(LogTemp, Log, TEXT("%s OnHitReceived: An existing montage was stopped to play hit reaction."),
			       *GetName());
		}


		AnimInstance->Montage_Play(VictimReactionMontageToPlay);
		UE_LOG(LogTemp, Log, TEXT("%s OnHitReceived: Playing received VictimReactionMontage: %s."), *GetName(),
		       *VictimReactionMontageToPlay->GetName());
	}
	else
	{
		if (!AnimInstance) UE_LOG(LogTemp, Warning, TEXT("%s OnHitReceived: Missing AnimInstance."), *GetName());
		if (!VictimReactionMontageToPlay) UE_LOG(LogTemp, Warning,
		                                         TEXT(
			                                         "%s OnHitReceived: VictimReactionMontageToPlay is NULL, cannot play reaction."
		                                         ), *GetName());
	}
}

TArray<AActor*> AMyCharacter::FindActorsInSphereToPull(const float SphereRadius, const TSubclassOf<UInterface> RequiredInterface,
	const bool bEnableDebugDraw)
{
	TArray<AActor*> FoundActors;
	const FVector SelfLocation = CurrentAttackTarget->GetActorLocation();

	TArray<FOverlapResult> OverlapResults;
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);
    
	const FCollisionObjectQueryParams ObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldDynamic) | ECC_TO_BITFIELD(ECC_Pawn));

	const bool bOverlap = GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		SelfLocation,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape
	);

	if (bEnableDebugDraw)
	{
		DrawDebugSphere(GetWorld(), SelfLocation, SphereRadius, 24, FColor::Blue, false, 5.0f, 0, 2.0f);
	}

	if (bOverlap)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* OverlappedActor = Result.GetActor();
			if (OverlappedActor && OverlappedActor != this && OverlappedActor != CurrentAttackTarget)
			{
				bool bInterfaceMatch = true;
				if (RequiredInterface)
				{
					bInterfaceMatch = OverlappedActor->GetClass()->ImplementsInterface(RequiredInterface);
				}

				if (bInterfaceMatch)
				{
					FoundActors.Add(OverlappedActor);
					if (bEnableDebugDraw)
					{
						DrawDebugLine(GetWorld(), SelfLocation, OverlappedActor->GetActorLocation(), FColor::Green, false, 5.0f, 0, 1.0f);
					}
				}
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[%s::FindActorsInSphereToPull] Found %d actors."), *GetName(), FoundActors.Num());
	return FoundActors;
}

bool AMyCharacter::PrepareGroupPull(const TArray<AActor*>& ActorsToPull, FVector TargetCentroidOffsetFromPlayer)
{
	    if (bIsGroupPullActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s::PrepareGroupPull] A group pull is already active."), *GetName());
        return false;
    }
    if (ActorsToPull.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s::PrepareGroupPull] ActorsToPull array is empty."), *GetName());
        return false;
    }

    ActivelyPulledActors.Empty();
    Pull_Group_InitialWorldCentroid = FVector::ZeroVector;
    int32 ValidActorCount = 0;

    for (const AActor* Actor : ActorsToPull)
    {
        if (Actor && Actor != this) 
        {
            Pull_Group_InitialWorldCentroid += Actor->GetActorLocation();
            ValidActorCount++;
        }
    }

    if (ValidActorCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s::PrepareGroupPull] No valid actors to form a group."), *GetName());
        return false;
    }
    Pull_Group_InitialWorldCentroid /= ValidActorCount;

    for (AActor* Actor : ActorsToPull)
    {
        if (Actor && Actor != this)
        {
            FVector Offset = Actor->GetActorLocation() - Pull_Group_InitialWorldCentroid;
            ActivelyPulledActors.Add(FPulledActorGroupInfo(Actor, Offset));
        }
    }
    
    if (ActivelyPulledActors.IsEmpty()) { 
        UE_LOG(LogTemp, Warning, TEXT("[%s::PrepareGroupPull] No actors were ultimately added to the pull list."), *GetName());
        return false;
    }

    Pull_Group_TargetWorldCentroid = GetActorLocation() + TargetCentroidOffsetFromPlayer;
    
    bIsGroupPullActive = true;
    UE_LOG(LogTemp, Log, TEXT("[%s::PrepareGroupPull] Prepared pull for %d actors. InitialCentroid: %s, TargetCentroid: %s"),
        *GetName(), ActivelyPulledActors.Num(), *Pull_Group_InitialWorldCentroid.ToString(), *Pull_Group_TargetWorldCentroid.ToString());
    return true;
}

void AMyCharacter::UpdateGroupPullLerp(float Alpha)
{
	if (!bIsGroupPullActive || ActivelyPulledActors.IsEmpty())
	{
		return;
	}

	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	const FVector CurrentLerpedCentroid = FMath::Lerp(Pull_Group_InitialWorldCentroid, Pull_Group_TargetWorldCentroid, ClampedAlpha);

	if (bIsDrawDebug) 
	{
		DrawDebugSphere(GetWorld(), Pull_Group_InitialWorldCentroid, 50.f, 12, FColor::Red, false, -1, 0, 2.f); 
		DrawDebugSphere(GetWorld(), Pull_Group_TargetWorldCentroid, 50.f, 12, FColor::Blue, false, -1, 0, 2.f); 
		DrawDebugSphere(GetWorld(), CurrentLerpedCentroid, 40.f, 12, FColor::Yellow, false, -1, 0, 3.f);
	}
	
	for (int32 i = ActivelyPulledActors.Num() - 1; i >= 0; --i) 
	{
		const FPulledActorGroupInfo& PulledInfo = ActivelyPulledActors[i];
		if (PulledInfo.Actor.IsValid())
		{
			AActor* TargetActor = PulledInfo.Actor.Get();
			FVector NewActorLocation = CurrentLerpedCentroid + PulledInfo.InitialOffsetFromCentroid;
			TargetActor->SetActorLocation(NewActorLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			ActivelyPulledActors.RemoveAt(i);
			UE_LOG(LogTemp, Warning, TEXT("[%s::UpdateGroupPullLerp] Pulled actor became invalid, removed from list."), *GetName());
		}
	}
}

void AMyCharacter::FinishGroupPull()
{
	if (bIsGroupPullActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s::FinishGroupPull] Group pull finished. Clearing active pull state."), *GetName());
	}
	bIsGroupPullActive = false;
	ActivelyPulledActors.Empty();
	Pull_Group_InitialWorldCentroid = FVector::ZeroVector;
	Pull_Group_TargetWorldCentroid = FVector::ZeroVector;
}

void AMyCharacter::OnMontageEndedEvent(UAnimMontage* Montage, bool bInterrupted)
{
	if (!CurrentExecutingComboName.IsNone())
	{
		const FCombatAnimationPair* CurrentAnimPair = CombatAnimationDatabase.Find(CurrentExecutingComboName);
		if (CurrentAnimPair && CurrentAnimPair->AttackerMontage == Montage)
		{
			UE_LOG(LogTemp, Log,
			       TEXT("%s AttackerMontage for combo '%s' ended. Interrupted: %d. Clearing target and combo name."),
			       *GetName(), *CurrentExecutingComboName.ToString(), bInterrupted);

			if (CurrentAttackTarget)
			{
				UE_LOG(LogTemp, Log, TEXT("%s performing '%s'. Target: %s"), *GetName(),
				       *CurrentExecutingComboName.ToString(), *CurrentAttackTarget->GetName());

				UCapsuleComponent* SelfCapsule = GetCapsuleComponent();
				UCapsuleComponent* TargetCapsule = CurrentAttackTarget->GetCapsuleComponent();

				if (SelfCapsule && TargetCapsule)
				{
					SelfCapsule->IgnoreActorWhenMoving(CurrentAttackTarget, true);
					TargetCapsule->IgnoreActorWhenMoving(this, true);

					UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Set %s and %s to ignore each other's movement collision."),
					       *GetName(), __FUNCTION__, *GetName(), *CurrentAttackTarget->GetName());
				}
				else
				{
					if (!SelfCapsule) UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - SelfCapsule is NULL."), *GetName(),
					                         __FUNCTION__);
					if (!TargetCapsule) UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - TargetCapsule on %s is NULL."),
					                           *GetName(), __FUNCTION__, *CurrentAttackTarget->GetName());
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("%s performing '%s' (no specific target)."), *GetName(),
				       *CurrentExecutingComboName.ToString());
			}

			CurrentAttackTarget = nullptr;
			CurrentExecutingComboName = NAME_None;
		}
	}
}
