// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatInterface.generated.h"

UINTERFACE()
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAS55_API ICombatInterface
{
	GENERATED_BODY()

public:
	
	// /**
	//  * Triggers a hit reaction animation or behavior in response to an attack.
	//  *
	//  * @param HitReactionMontageSection The name of the montage section to trigger for the hit reaction.
	//  * @param Attacker The actor that initiated the attack causing the hit reaction.
	//  * @param HitImpactPoint The location in the game world where the hit impact occurred.
	//  */
	// UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	// void TriggerHitReaction(FName HitReactionMontageSection, AActor* Attacker, FVector HitImpactPoint);

	/**
	 * Handles the event when an actor receives a hit, triggering the appropriate reaction.
	 *
	 * @param Attacker The actor responsible for initiating the hit.
	 * @param VictimReactionMontageToPlay The animation montage to be played as the victim's reaction.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	void OnHitReceived(AActor* Attacker, UAnimMontage* VictimReactionMontageToPlay);
};