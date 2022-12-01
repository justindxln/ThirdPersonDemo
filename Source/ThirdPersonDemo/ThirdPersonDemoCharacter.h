// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ThirdPersonDemoCharacter.generated.h"

class UAnimMontage;

UCLASS(config=Game)
class AThirdPersonDemoCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	AThirdPersonDemoCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

protected:

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:

	UPROPERTY(EditAnywhere, Category = "Anim Montages")
	UAnimMontage* ClimbMontage;

	UPROPERTY(EditAnywhere, Category = "Debug Toggle")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float ClimbForwardDistance = 50;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float ClimbUpMaxDistance = 100;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float ClimbUpMinDistance = 30;

	UPROPERTY(EditAnywhere, Category = "Refinement tweaks")
	float HangHorizontalOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Refinement tweaks")
	float HangVerticalOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Refinement tweaks")
	float TraceShapeRadius = 10.f;

	FHitResult TraceClimbForwardResult;
	FHitResult TraceClimbUpResult;

	bool bIsHanging = false;
	bool bIsClimbing = false;

	virtual void Tick(float DeltaSeconds) override;

	void TryHang();

	void TryClimbUp();

	void OnClimbUpFinished();

	void TryDropDown();

	/** Trace forward to check geometry for climbing **/
	bool TraceForwardClimb();

	/** Trace forward to check geometry for climbing **/
	bool TraceUpClimb();
};

