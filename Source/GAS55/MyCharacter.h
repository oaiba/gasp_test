// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interface/CombatInterface.h"

#include "MyCharacter.generated.h"

USTRUCT(BlueprintType)
struct FCombatAnimationPair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation")
	UAnimMontage* AttackerMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation")
	UAnimMontage* VictimReactionMontage;

	/**
	 * Relative transform of the victim compared to the attacker at the interaction moment.
	 * Used to "warp" (adjust position/rotation) the victim to match the attack animation.
	 * The attacker is considered as origin (0,0,0) with an X direction being forward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation")
	FTransform VictimRelativeTransformToAttacker;

	FCombatAnimationPair()
		: AttackerMontage(nullptr)
		  , VictimReactionMontage(nullptr)
		  , VictimRelativeTransformToAttacker(FTransform::Identity)
	{
	}
};

UCLASS()
class GAS55_API AMyCharacter : public ACharacter, public ICombatInterface
{
	GENERATED_BODY()

public:
	AMyCharacter();

protected:
	virtual void BeginPlay() override;

	// --- Combat Properties ---
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation Data")
	TMap<FName, FCombatAnimationPair> CombatAnimationDatabase;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FName CurrentExecutingComboName;
	
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	AMyCharacter* CurrentAttackTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Debug")
	bool bIsDrawDebug = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackDetectionRange = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackDetectionAngle = 45.0f;

	// --- End Combat Properties ---

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	FTransform TargetRelativeTransform;
	
	/**
	 * Finds all AMyCharacter instances (excluding self) that are within a specified yaw angle
	 * (horizontal cone) and range relative to this character's forward direction.
	 * @param MaxYawAngleDegrees The half-angle of the cone in degrees (e.g., 45.0f means +/- 45 degrees from forward).
	 * @param MaxRange The maximum distance to check for characters.
	 * @param bInDrawDebug If true, debug visualizations for the cone, range, and checked characters will be drawn.
	 * @return An array of AMyCharacter pointers that meet all criteria.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Queries")
	TArray<AMyCharacter*> FindCharactersInYawAngle(const float MaxYawAngleDegrees = 45.0f,
	                                               const float MaxRange = 5000.0f, const bool bInDrawDebug = false);

	/**
	 * Selects the best attack target from a list of potential targets based on proximity and other criteria.
	 * Ensures the selected target is valid and optionally draws debug information if enabled.
	 *
	 * @param PotentialTargets An array of potential target AMyCharacter instances to evaluate.
	 * @param bInDrawDebug If true, debugging information about the target selection process will be visualized.
	 * @return A pointer to the best target AMyCharacter, or nullptr if no valid target is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	AMyCharacter* SelectBestAttackTargetFromList(const TArray<AMyCharacter*>& PotentialTargets,
	                                            bool bInDrawDebug = false);

	/**
	 * Executes an attack action for the character based on a provided attack code.
	 * Determines the appropriate attack animation and potential targets within range and selection criteria.
	 * Plays the attack animation if a valid montage is found, and logs the action performed.
	 *
	 * @param AttackCode The identifier of the attack to be executed, corresponding to an entry in the CombatAnimationDatabase.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	void PerformAttack(const FName& AttackCode);


	/**
	 * Processes the aftermath of a successful attack hit on the current target.
	 *
	 * This method ensures that when an attack hits a target, the appropriate animation, transformation,
	 * and interface calls are executed. It validates the existence of a target, ensures the combo data
	 * is available, and applies transformations or animations if necessary. It also handles the logic
	 * if the target implements the combat interface.
	 *
	 * Key behaviors:
	 * - Logs warnings or errors if critical data (e.g., target or combo information) is missing.
	 * - Applies relative transformations to the target if specified in the combo data.
	 * - Executes the target's on-hit reaction via its combat interface implementation if supported.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	void ProcessAttackHit();
	
	/**
	 * Adjusts the victim's position and rotation to align with a relative transform based on the attacker's current transform.
	 * This is typically used to synchronize the victim's position/orientation with the attack animation.
	 *
	 * @param Victim The victim character to apply the relative transform to. Can be nullptr, in which case no action is taken.
	 * @param RelativeTransform The transform that specifies the victim's position and orientation relative to the attacker.
	 */
	UFUNCTION(BlueprintCallable)
	void ApplyVictimRelativeTransform(AMyCharacter* Victim, const FTransform& RelativeTransform) const;
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnHandleApplyVictimRelativeTransform(const FTransform& RelativeTransform);

	UFUNCTION(BlueprintCallable)
	void HandleApplyVictimRelativeTransform();

	// virtual void TriggerHitReaction_Implementation(FName HitReactionMontageSection, AActor* Attacker, FVector HitImpactPoint) override;
	
	/**
	 * Handles the reaction of this character when hit by an attacker.
	 * Plays the specified reaction animation montage if valid and stops any conflicting montages.
	 *
	 * @param Attacker The actor that initiated the attack. Can be used for contextual actions or logging.
	 * @param VictimReactionMontageToPlay The animation montage to play as this character's reaction to the hit.
	 *                                    If null, no animation will be played.
	 */
	virtual void OnHitReceived_Implementation(AActor* Attacker, UAnimMontage* VictimReactionMontageToPlay) override;

protected:
	/**
	 * Handles the event when an animation montage has finished playing.
	 * This function is triggered whenever the montage completes or is interrupted.
	 * Updates the combat state by clearing the current combo and attack target if the montage
	 * corresponds to the active combat sequence.
	 *
	 * @param Montage The animation montage that has ended.
	 * @param bInterrupted True if the montage was interrupted, false if it completed normally.
	 */
	UFUNCTION()
	void OnMontageEndedEvent(UAnimMontage* Montage, bool bInterrupted);
};
