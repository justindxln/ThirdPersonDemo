// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonDemoCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"

//////////////////////////////////////////////////////////////////////////
// AThirdPersonDemoCharacter

AThirdPersonDemoCharacter::AThirdPersonDemoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

void AThirdPersonDemoCharacter::BeginPlay()
{
	Super::BeginPlay();

	MaxJumpHeight = GetCharacterMovement()->GetMaxJumpHeight();
	CameraBoomOriginalLength = CameraBoom->TargetArmLength;
	RecalculateTargetCameraOffset();
}

void AThirdPersonDemoCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Movecharacter();
	TryUIHang();
	TryHang();
	TryEnterWallRun();
	AdjustCameraOffset(DeltaSeconds);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AThirdPersonDemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward");
	PlayerInputComponent->BindAxis("MoveRight");

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AThirdPersonDemoCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("ClimbUp", IE_Pressed, this, &AThirdPersonDemoCharacter::TryClimbUp);
	PlayerInputComponent->BindAction("DropDown", IE_Pressed, this, &AThirdPersonDemoCharacter::TryDropDown);
	PlayerInputComponent->BindAction("ToggleCover", IE_Pressed, this, &AThirdPersonDemoCharacter::ToggleCover);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AThirdPersonDemoCharacter::StartAim);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AThirdPersonDemoCharacter::EndAim);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &AThirdPersonDemoCharacter::Turn);
	PlayerInputComponent->BindAxis("TurnRate", this, &AThirdPersonDemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AThirdPersonDemoCharacter::LookUpAtRate);
}

void AThirdPersonDemoCharacter::Jump()
{
	// If wallrunning, jump off wall at an angle
	if (bIsWallRunning)
	{
		GetCharacterMovement()->Velocity = RotateAngleZAxis(GetActorForwardVector(), !bIsRightWallRunning, 45.f) * WallRunMinJumpOffSpeed;
	}

	Super::Jump();
}

bool AThirdPersonDemoCharacter::CanJumpInternal_Implementation() const
{
	const bool bCanJump = bIsWallRunning || (!bIsHanging && !bIsClimbing && !bIsInCover && Super::CanJumpInternal_Implementation());

	return bCanJump;
}

void AThirdPersonDemoCharacter::OnResetVR()
{
	// If ThirdPersonDemo is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in ThirdPersonDemo.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AThirdPersonDemoCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AThirdPersonDemoCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AThirdPersonDemoCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	Turn(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonDemoCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonDemoCharacter::Movecharacter()
{
	if (Controller == nullptr) return;

	// Get movement vector from inputs, rotate by controller yaw to get world direction, then normalise magnitude to 1
	ControlMoveVector = FVector(GetInputAxisValue("MoveForward"), GetInputAxisValue("MoveRight"), 0.f);
	ControlMoveVector = RotateAngleZAxis(ControlMoveVector, true, GetControlRotation().Yaw);
	ControlMoveVector.Normalize();

	// Get movement magnitude from vector
	ControlMoveMagnitude = ControlMoveVector.Size();

	if (bIsWallRunning)
	{
		MoveCharacterWallRun();
	}
	else
	{
		MoveCharacterDefault();
	}
}

void AThirdPersonDemoCharacter::MoveCharacterDefault()
{
	if (bIsHanging || (bIsInCover && !bIsAiming)) return;
	if (ControlMoveMagnitude == 0.0f) return;

	// Clamp movement speed it if player is aiming when walking
	if (bIsAiming && !GetCharacterMovement()->IsFalling()) 
	{
		ControlMoveMagnitude = FMath::Min(ControlMoveMagnitude, MaxAimMoveRate);
	}
	AddMovementInput(ControlMoveVector, ControlMoveMagnitude);
		
	if (!bIsAiming) return;

	// Keep character facing front when walking while aiming
	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Yaw = GetControlRotation().Yaw;
	SetActorRotation(ActorRotation.Quaternion());

	// If already popping out from cover, exit cover
	if (bIsInCover) ExitCover();
}

void AThirdPersonDemoCharacter::MoveCharacterWallRun()
{
	// If there is no more wall or the character touches the floor, exit wallrun
	if (!TraceSideWallRun() || TraceDownWallRun())
	{
		ExitWallRun();
		return;
	}

	// If there is no input in the wallrun direction, exit wallrun
	FVector WallRunDirection = RotateAngleZAxis(TraceSideWallRunResult.Normal, bIsRightWallRunning);
	if (ControlMoveMagnitude <= 0.1f || FVector::DotProduct(ControlMoveVector, WallRunDirection) <= 0.0f)
	{
		ExitWallRun();
		return;
	}

	// If the character moves too slowly, exit wallrun
	if (GetHorizontalVector(GetCharacterMovement()->Velocity).Size() < WallRunMinHorizontalSpeed)
	{
		ExitWallRun();
		return;
	}

	float CorrectionAngle = (FVector::PointPlaneDist(GetActorLocation(), TraceSideWallRunResult.Location, TraceSideWallRunResult.Normal) - WallRunOffset) / 2;
	WallRunDirection = RotateAngleZAxis(WallRunDirection, bIsRightWallRunning, CorrectionAngle);

	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Offset Difference %f"), CorrectionAngle));
	AddMovementInput(WallRunDirection, ControlMoveMagnitude);
}

void AThirdPersonDemoCharacter::Turn(float Rate)
{ 
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate);

	if (!bIsAiming) return;
		
	const float DeltaYaw = UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), GetActorRotation()).Yaw;
	if (UKismetMathLibrary::InRange_FloatFloat(DeltaYaw, -90.f, 90.f)) return;

	const float RelativeYaw = DeltaYaw < -90.f ? DeltaYaw + 90.f : DeltaYaw - 90.f;
	SetActorRotation((GetActorRotation() + FRotator(0.f, RelativeYaw, 0.f)).Quaternion());
}

//////////////////////////////////////////////////////////////////////////
// Camera Control

void AThirdPersonDemoCharacter::AdjustCameraOffset(const float DeltaSeconds)
{
	CameraBoom->SocketOffset = UKismetMathLibrary::VInterpTo(CameraBoom->SocketOffset, CameraOffset, DeltaSeconds, CameraOffsetSpeed);
	CameraBoom->TargetArmLength = UKismetMathLibrary::FInterpTo(CameraBoom->TargetArmLength, CameraBoomLength, DeltaSeconds, CameraOffsetSpeed);
}

void AThirdPersonDemoCharacter::RecalculateTargetCameraOffset()
{
	if (bIsAiming)
	{
		CameraOffset = FVector(0.f, bIsRightCover ? CameraAimYOffset : -CameraAimYOffset, 0.f);
		CameraBoomLength = CameraBoomAimLength;
	}
	else if (bIsInCover && bIsTallCover)
	{
		CameraOffset = FVector(0.f, bIsRightCover ? CameraCoverYOffset : -CameraCoverYOffset, 0.f);
		CameraBoomLength = CameraBoomOriginalLength;
	}
	else
	{
		CameraOffset = FVector::ZeroVector;
		CameraBoomLength = CameraBoomOriginalLength;
	}
}

void AThirdPersonDemoCharacter::StartAim()
{
	if (bIsInCover)
	{
		FVector ActorLocation = GetActorLocation();
		if (bIsTallCover) ActorLocation += RotateAngleZAxis(GetActorForwardVector(), !bIsRightCover) * CoverAimYOffset;

		MoveCapsuleComponentTo(ActorLocation, GetCoverRotation() + FRotator(0.f, 180.f, 0.f));
	}
	
	//bUseControllerRotationYaw = true;
	bIsAiming = true;
	RecalculateTargetCameraOffset();
}

void AThirdPersonDemoCharacter::EndAim()
{
	if (bIsInCover)
	{
		MoveCapsuleComponentTo(GetCoverLocation(), GetCoverRotation());
	}

	bUseControllerRotationYaw = false;
	bIsAiming = false;
	RecalculateTargetCameraOffset();
}

//////////////////////////////////////////////////////////////////////////
// In-World UI

void AThirdPersonDemoCharacter::TryUIHang()
{
	if (bIsInCover || GetCharacterMovement()->IsFalling()) return;

	bool bCanShowHangUI = !bIsHanging && TraceUpClimb(true) && TraceForwardClimb(true) && UKismetMathLibrary::InRange_FloatFloat(TraceUpClimbResult.Location.Z - GetActorLocation().Z, ClimbUpMinDistance, ClimbUpMaxDistance + MaxJumpHeight);

	// If there is a climbable object in range, show the UI Actor
	if (bCanShowHangUI)
	{
		// If UI Actor doesn't exist, spawn it. If not, move the existing on to the new location
		FVector UILocation = TraceForwardClimbResult.Location;
		UILocation.Z = TraceUpClimbResult.Location.Z;
		UILocation = UILocation - TraceForwardClimbResult.Normal * TraceOffset;

		if (CurrentClimbUI == nullptr)
		{
			const FRotator UIRotation = TraceForwardClimbResult.Normal.Rotation();
			CurrentClimbUI = GetWorld()->SpawnActor<AActor>(ClimbUIClass, UILocation, UIRotation);
		}
		else
		{
			CurrentClimbUI->SetActorLocation(UILocation);
		}
	}
	// If there is no climbable object, remove the current UI Actor if it exists
	else
	{
		if (CurrentClimbUI != nullptr)
		{
			CurrentClimbUI->Destroy();
			CurrentClimbUI = nullptr;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Hanging/Climbing

void AThirdPersonDemoCharacter::TryHang()
{
	// Return if character is not in the right state for wall hang or no ledge is available
	if (!(GetCharacterMovement()->IsFalling()) || bIsWallRunning || bIsHanging || !TraceUpClimb() || !TraceForwardClimb()) return;

	// Return if ledge is not within the right height range
	if (!UKismetMathLibrary::InRange_FloatFloat(TraceUpClimbResult.Location.Z - GetActorLocation().Z, ClimbUpMinDistance, ClimbUpMaxDistance)) return;

	bIsHanging = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();

	FVector HangLocation = TraceForwardClimbResult.Location + TraceForwardClimbResult.Normal * HangHorizontalOffset;
	HangLocation.Z = TraceUpClimbResult.Location.Z - HangVerticalOffset;
	const FRotator HangRotation = UKismetMathLibrary::MakeRotFromX(TraceForwardClimbResult.Normal * -1);

	MoveCapsuleComponentTo(HangLocation, HangRotation);
}

void AThirdPersonDemoCharacter::TryClimbUp()
{
	if (bIsClimbing || !bIsHanging) return;

	// Play climbing animation montage and set timer for "callback" when animation ends
	const float AnimDuration = PlayAnimMontage(ClimbMontage);
	FTimerHandle ClimbUpTimerHandle;
	GetWorldTimerManager().SetTimer(ClimbUpTimerHandle, this, &AThirdPersonDemoCharacter::OnClimbUpFinished, AnimDuration - ClimbMontage->BlendOutTriggerTime);
	bIsClimbing = true;
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Moving in %f"), AnimDuration - ClimbMontage->BlendOutTriggerTime));
}

void AThirdPersonDemoCharacter::OnClimbUpFinished()
{
	// Move the character forward after climbing
	const FVector EndLocation = GetActorLocation() + GetActorForwardVector() * 30.f;
	MoveCapsuleComponentTo(EndLocation, GetActorRotation());

	bIsHanging = false;
	bIsClimbing = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AThirdPersonDemoCharacter::TryDropDown()
{
	if (!bIsHanging || bIsClimbing) return;

	// Exit hang animation and set state to falling
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	bIsHanging = false;
}

//////////////////////////////////////////////////////////////////////////
// WallRun

void AThirdPersonDemoCharacter::TryEnterWallRun()
{
	// Return if character is not in the air or already wallrunning
	if (!GetCharacterMovement()->IsFalling() || bIsWallRunning ) return;

	// Return if character is falling too fast or horizontal move speed is too slow for wallrun
	const FVector HorizontalVector = GetHorizontalVector(GetCharacterMovement()->Velocity);
	const float VerticalVelocity = GetCharacterMovement()->Velocity.Z;
	if (HorizontalVector.Size() < WallRunMinHorizontalSpeed || VerticalVelocity < WallRunMinVerticalVelocity) return;

	// Return if there is no available wall or player is not controlling character to move in the same direction as the wallrun
	FVector WallRunDirection = RotateAngleZAxis(TraceSideWallRunResult.Normal, bIsRightWallRunning);
	if (!TraceSideWallRun() || ControlMoveMagnitude <= 0.1f || FVector::DotProduct(ControlMoveVector, WallRunDirection) <= 0.0f) return;

	// Set appropriate initial Z velocity and gravity scale for wallrun
	GetCharacterMovement()->GravityScale = WallRunMinGravityScale;
	GetCharacterMovement()->Velocity.Z *= WallRunVerticalSpeedMultiplier;
	bIsWallRunning = true;
}

void AThirdPersonDemoCharacter::ExitWallRun()
{
	GetCharacterMovement()->GravityScale = 1.f;
	bIsWallRunning = false;
}

//////////////////////////////////////////////////////////////////////////
// Taking Cover

void AThirdPersonDemoCharacter::ToggleCover()
{
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Walking) return;

	if (!bIsInCover)
	{
		TryEnterCover();
	}
	else 
	{
		ExitCover();
	}
}

void AThirdPersonDemoCharacter::TryEnterCover()
{
	// Check if there is a valid wall in front of the player to take cover against
	if (!TraceForwardCover()) return;

	// Get the angle of the line trace against the wall to determine if peeking out of the left or right cover
	const FVector2D CharacterForwardVector = FVector2D(GetActorForwardVector().X, GetActorForwardVector().Y);
	const FVector2D WallNormal = FVector2D(TraceForwardCoverResult.Normal.X, TraceForwardCoverResult.Normal.Y);
	const float DotProduct = FVector2D::DotProduct(CharacterForwardVector, WallNormal);
	const float Determinant = CharacterForwardVector.X * WallNormal.Y - CharacterForwardVector.Y * WallNormal.X;
	bIsRightCover = atan2(Determinant, DotProduct) > 0;

	// If it's a tall cover, check where it ends on the left/right side to take cover there
	if (bIsTallCover && !TraceSideCover())
	{
		bIsRightCover = !bIsRightCover;
		if (!TraceSideCover()) return;
	}

	// Ideally play an animation of getting into cover but it's a little janky right now
	/*
	const float AnimDuration = PlayAnimMontage(EnterCoverRightMontage);
	FTimerHandle CoverTimerHandle;
	GetWorldTimerManager().SetTimer(CoverTimerHandle, this, &AThirdPersonDemoCharacter::OnEnterCoverFinished, AnimDuration - EnterCoverRightMontage->BlendOutTriggerTime);
	*/

	// Set the booleans for cover state and move character to cover location
	bIsInCover = true;
	bIsAiming = false;
	MoveCapsuleComponentTo(GetCoverLocation(), GetCoverRotation());
	RecalculateTargetCameraOffset();
}

void AThirdPersonDemoCharacter::ExitCover()
{
	bIsInCover = false;
	RecalculateTargetCameraOffset();
}

//////////////////////////////////////////////////////////////////////////
// Line Trace Functions

bool AThirdPersonDemoCharacter::TraceUpClimb(const bool bDisableDraw /*= false*/)
{
	// Check if there is a platform of the right height in front of the player to hang on
	const FVector TraceEnd = GetActorLocation() + GetActorForwardVector() * ClimbForwardDistance + FVector::UpVector * ClimbUpMinDistance;
	const FVector TraceStart = TraceEnd + FVector::UpVector * (ClimbUpMaxDistance + MaxJumpHeight);

	return DoLineTraceCheck(TraceStart, TraceEnd, TraceUpClimbResult, bDisableDraw);
}

bool AThirdPersonDemoCharacter::TraceForwardClimb(const bool bDisableDraw /*= false*/)
{
	// Check if there is a platform of the right distance in front of the player to hang on
	const FVector TraceStart = FVector(GetActorLocation().X, GetActorLocation().Y, TraceUpClimbResult.Location.Z - TraceOffset * 2);
	const FVector TraceEnd = TraceStart + GetActorForwardVector() * ClimbForwardDistance;

	return DoLineTraceCheck(TraceStart, TraceEnd, TraceForwardClimbResult, bDisableDraw);
}

bool AThirdPersonDemoCharacter::TraceSideWallRun()
{
	const FVector TraceStart = GetActorLocation() + FVector::DownVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere();
	FVector TraceEnd;

	// If already wallrunning, keep checking if a wall is available on the current side
	if (bIsWallRunning)
	{
		TraceEnd = TraceStart + RotateAngleZAxis(GetActorForwardVector(), bIsRightWallRunning) * WallRunSideDistance;
		return DoLineTraceCheck(TraceStart, TraceEnd, TraceSideWallRunResult);
	}

	// If not already wallrunning, first check for a wall on the right side
	TraceEnd = TraceStart + RotateAngleZAxis(GetActorForwardVector(), true) * WallRunSideDistance;
	if (DoLineTraceCheck(TraceStart, TraceEnd, TraceSideWallRunResult))
	{
		bIsRightWallRunning = true;
		return true;
	}

	// If no wall on the right side is available, check the left side
	TraceEnd = TraceStart + RotateAngleZAxis(GetActorForwardVector(), false) * WallRunSideDistance;
	bIsRightWallRunning = false;
	return DoLineTraceCheck(TraceStart, TraceEnd, TraceSideWallRunResult);

}

bool AThirdPersonDemoCharacter::TraceDownWallRun()
{
	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + FVector::DownVector * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f);
	FHitResult HitResult;

	return DoLineTraceCheck(TraceStart, TraceEnd, HitResult);
}

bool AThirdPersonDemoCharacter::TraceForwardCover()
{
	// First check for tall wall cover
	FVector TraceStart = GetActorLocation() + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere();
	FVector TraceEnd = TraceStart + GetActorForwardVector() * CoverForwardDistance;
	if (DoLineTraceCheck(TraceStart, TraceEnd, TraceForwardCoverResult))
	{
		bIsTallCover = true;
		return true;
	}

	// If no tall cover is available, check for a short wall cover
	bIsTallCover = false;
	TraceStart = GetActorLocation();
	TraceEnd = TraceStart + GetActorForwardVector() * CoverForwardDistance;
	return DoLineTraceCheck(TraceStart, TraceEnd, TraceForwardCoverResult);
}

bool AThirdPersonDemoCharacter::TraceSideCover()
{
	// Check where the wall cover ends
	const FVector TraceEnd = TraceForwardCoverResult.Location - TraceForwardCoverResult.Normal * TraceOffset;
	const FVector TraceStart = TraceEnd + RotateAngleZAxis(TraceForwardCoverResult.Normal, !bIsRightCover) * CoverSideDistance;

	return DoLineTraceCheck(TraceStart, TraceEnd, TraceSideCoverResult);
}

//////////////////////////////////////////////////////////////////////////
// Helper Functions

bool AThirdPersonDemoCharacter::DoLineTraceCheck(const FVector TraceStart, const FVector TraceEnd, FHitResult& OutHit, const bool bDisableDraw /*= false*/)
{
	const EDrawDebugTrace::Type DrawMode = (bDrawDebug && !bDisableDraw) ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;
	return UKismetSystemLibrary::LineTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceTypeQuery_MAX, false, { this }, DrawMode, OutHit, true);
}

void AThirdPersonDemoCharacter::MoveCapsuleComponentTo(const FVector TargetLocation, const FRotator TargetRotation, const float OverTime /*= 0.2f*/)
{
	FLatentActionInfo LatentActionInfo;
	LatentActionInfo.CallbackTarget = this;
	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), TargetLocation, TargetRotation, true, true, 0.2f, true, EMoveComponentAction::Move, LatentActionInfo);
}

FVector AThirdPersonDemoCharacter::GetCoverLocation() const
{
	if (bIsTallCover)
	{
		return TraceSideCoverResult.Location - TraceSideCoverResult.Normal * CoverSideOffset 
			+ TraceForwardCoverResult.Normal * (CoverForwardOffset + TraceOffset) 
			+ FVector::DownVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere();
	}
	else
	{
		return TraceForwardCoverResult.Location + TraceForwardCoverResult.Normal * CoverForwardOffset;
	}
}

FRotator AThirdPersonDemoCharacter::GetCoverRotation() const
{
	return TraceForwardCoverResult.Normal.Rotation();
}

FVector AThirdPersonDemoCharacter::RotateAngleZAxis(const FVector InVector, bool bClockWise, float Degree /*= 90.f*/) const
{
	return UKismetMathLibrary::RotateAngleAxis(InVector, bClockWise ? Degree : -Degree, FVector::UpVector);
}

FVector AThirdPersonDemoCharacter::GetHorizontalVector(const FVector InVector) const
{
	FVector OutVector = InVector;
	OutVector.Z = 0;
	return OutVector;
}
