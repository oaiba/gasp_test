// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"

#include "GameFramework/PawnMovementComponent.h"
#include "Interface/CombatInterface.h"
#include "Kismet/GameplayStatics.h"

AMyCharacter::AMyCharacter(): CurrentAttackTarget(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &AMyCharacter::OnMontageEndedEvent);
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
			       "SelectBestAttackTargetFromList: No valid targets in PotentialTargets after checking for nullptrs. Returning nullptr."
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
	const FTransform TargetTransform = FTransform(VictimTargetWorldTransform.GetRotation(), VictimTargetWorldTransform.GetLocation(),
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
		CurrentAttackTarget->SetActorLocationAndRotation(RelativeTransform.GetLocation(), RelativeTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
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

			CurrentAttackTarget = nullptr;
			CurrentExecutingComboName = NAME_None;
		}
	}
}
