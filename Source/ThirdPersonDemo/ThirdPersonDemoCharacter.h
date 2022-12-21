// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
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

	virtual void BeginPlay() override;

	virtual bool CanJumpInternal_Implementation() const override;

	virtual void Jump() override;

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

	UPROPERTY(EditAnywhere, Category = "3D UI Blueprints")
	TSubclassOf<AActor> ClimbUIClass;

	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float MaxAimMoveRate = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float ClimbForwardDistance = 50;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float ClimbUpMaxDistance = 100;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float ClimbUpMinDistance = 30;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float WallRunSideDistance = 30;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float WallRunMinHorizontalSpeed = 500;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float WallRunMinVerticalVelocity = -100;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float WallRunMinJumpOffSpeed = 800.f;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float WallRunMinGravityScale = 0.15f;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float WallRunVerticalSpeedMultiplier = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float CoverForwardDistance = 100;
	UPROPERTY(EditAnywhere, Category = "Traversal Properties")
	float CoverSideDistance = 100;

	UPROPERTY(EditAnywhere, Category = "Traversal tweaks")
	float HangHorizontalOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Traversal tweaks")
	float HangVerticalOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Traversal tweaks")
	float WallRunOffset = 45.f;
	UPROPERTY(EditAnywhere, Category = "Traversal tweaks")
	float CoverForwardOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Traversal tweaks")
	float CoverSideOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Traversal tweaks")
	float CoverAimYOffset = 50.f;

	UPROPERTY(EditAnywhere, Category = "Camera Control tweaks")
	float CameraCoverYOffset = 50.f;
	UPROPERTY(EditAnywhere, Category = "Camera Control tweaks")
	float CameraAimYOffset = 30.f;
	UPROPERTY(EditAnywhere, Category = "Camera Control tweaks")
	float CameraOffsetSpeed = 100.f;
	UPROPERTY(EditAnywhere, Category = "Camera Control tweaks")
	float CameraBoomAimLength = 150.f;
	UPROPERTY(EditAnywhere, Category = "Camera Control tweaks")
	float TraceOffset = 10.f;

	float MaxJumpHeight;
	float CameraBoomOriginalLength;

	AActor* CurrentClimbUI;

	FVector CameraOffset;
	float CameraOffsetFOV;
	float CameraBoomLength;

	FVector ControlMoveVector;
	float ControlMoveMagnitude;

	FHitResult TraceForwardClimbResult;
	FHitResult TraceUpClimbResult;
	FHitResult TraceSideWallRunResult;
	FHitResult TraceForwardCoverResult;
	FHitResult TraceSideCoverResult;

	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsAiming;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsHanging;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsClimbing;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsInCover;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsRightCover;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsTallCover;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsWallRunning;
	UPROPERTY(BlueprintReadOnly, Category = "Movement State")
	bool bIsRightWallRunning;

	virtual void Tick(float DeltaSeconds) override;

	/** Called every tick to move the character **/
	void Movecharacter();

	/** Default movement control when not in any special state **/
	void MoveCharacterDefault();

	/** Control movement during wallrunning **/
	void MoveCharacterWallRun();

	/** Called via input to turn the camera. Also turn the actor if needed when aiming **/
	void Turn(float Rate);

	/** Called every tick to adjust camera position based on current target camera offset **/
	void AdjustCameraOffset(const float DeltaSeconds);

	/** Set target camera offset based on current character state **/
	void RecalculateTargetCameraOffset();

	/* Zoom camera to aim */
	void StartAim();

	/* Undo zoom camera*/
	void EndAim();

	/** Check if ledge is available in range and display indicator UI **/
	void TryUIHang();

	/** Check if ledge is available in range and enter hang state if possible **/
	void TryHang();

	/** Climbup ledge from hanging state **/
	void TryClimbUp();

	/** Call when climbing up animation has finished **/
	void OnClimbUpFinished();

	/** Drop down from hanging state **/
	void TryDropDown();

	/** Check if wall is available in range and enter wall run state if possible **/
	void TryEnterWallRun();

	/** Exit wall run state **/
	void ExitWallRun();

	/** Toggle in/out of cover state **/
	void ToggleCover();

	/** Check if cover is available in range and enter cover state if possible **/
	void TryEnterCover();

	/** Exit cover state **/
	void ExitCover();

	/** Trace forward to check geometry for climbing **/
	bool TraceForwardClimb(const bool bDisableDraw = false);

	/** Trace downward to check geometry for climbing **/
	bool TraceUpClimb(const bool bDisableDraw = false);

	/** Trace to the side to check geometry for wall running **/
	bool TraceSideWallRun();

	/** Trace downward to check for ground to exit wall running **/
	bool TraceDownWallRun();

	/** Trace forward to check geometry for entering cover **/
	bool TraceForwardCover();

	/** Trace left to check geometry for entering cover **/
	bool TraceSideCover();

	//////////////////////////////////////////////////////////////////////////
	// Helper Functions

	/** Helper function for Line Traces **/
	bool DoLineTraceCheck(const FVector TraceStart, const FVector TraceEnd, FHitResult& OutHit, const bool bDisableDraw = false);

	/** Helper function for MoveComponentTo for Capsule Component **/
	void MoveCapsuleComponentTo(const FVector TargetLocation, const FRotator TargetRotation, const float OverTime = 0.2f);

	/** Helper function to get cover location **/
	FVector GetCoverLocation() const;

	/** Helper function to get cover rotation **/
	FRotator GetCoverRotation() const;

	/** Helper function to rotate Vector 90 degress around Z axis **/
	FVector RotateAngleZAxis(const FVector InVector, bool bClockWise, float Degree = 90.f) const;

	/** Helper function to get vector with zero vertical component **/
	FVector GetHorizontalVector(const FVector InVector) const;
};

