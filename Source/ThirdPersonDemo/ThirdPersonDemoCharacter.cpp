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

//////////////////////////////////////////////////////////////////////////
// Input

void AThirdPersonDemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	//PlayerInputComponent->BindAxis("MoveForward", this, &AThirdPersonDemoCharacter::MoveForward);
	//PlayerInputComponent->BindAxis("MoveRight", this, &AThirdPersonDemoCharacter::MoveRight);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
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

void AThirdPersonDemoCharacter::BeginPlay()
{
	Super::BeginPlay();

	MaxJumpHeight = GetCharacterMovement()->GetMaxJumpHeight();
	CameraBoomOriginalLength = CameraBoom->TargetArmLength;
	RecalculateTargetCameraOffset();
}

bool AThirdPersonDemoCharacter::CanJumpInternal_Implementation() const
{
	const bool bCanJump = !bIsHanging && !bIsClimbing && !bIsInCover;

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

void AThirdPersonDemoCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Movecharacter(DeltaSeconds);
	TryUIHang();
	TryHang();
	AdjustCameraOffset(DeltaSeconds);

	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("POS: %f, %f, %f"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z));
	//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("%s"), GetCharacterMovement()->IsFalling() ? TEXT("true") : TEXT("false")));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Forward Vector: %f, %f, %f"), GetActorForwardVector().X, GetActorForwardVector().Y, GetActorForwardVector().Z));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Location: %f, %f, %f"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z));
}

void AThirdPersonDemoCharacter::MoveForward(float Value)
{
	if (bIsHanging || (bIsInCover && !bIsAiming)) return;

	if ((Controller == nullptr) || (Value == 0.0f)) return;

	// find out which way is forward
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// limit max walk speed if aiming
	if (bIsAiming && !GetCharacterMovement()->IsFalling()) Value = FMath::Clamp(Value, -MaxAimMoveRate, MaxAimMoveRate);

	// get forward vector
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(Direction, Value);

	// If already popping out from cover, exit cover
	if (bIsInCover && bIsAiming) ExitCover();

	// Keep character facing front when walking while aiming
	if (!bIsAiming) return;
	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Yaw = GetControlRotation().Yaw;
	SetActorRotation(ActorRotation.Quaternion());

}

void AThirdPersonDemoCharacter::MoveRight(float Value)
{
	if (bIsHanging || (bIsInCover && !bIsAiming)) return;

	if ((Controller == nullptr) || (Value == 0.0f)) return;
	
	// find out which way is right
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// limit max walk speed if aiming
	if (bIsAiming && !GetCharacterMovement()->IsFalling()) Value = FMath::Clamp(Value, -MaxAimMoveRate, MaxAimMoveRate);

	// get right vector 
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	// add movement in that direction
	AddMovementInput(Direction, Value);

	// If already popping out from cover, exit cover
	if (bIsInCover && bIsAiming) ExitCover();

	// Keep character facing front when walking while aiming
	if (!bIsAiming) return;
	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Yaw = GetControlRotation().Yaw;
	SetActorRotation(ActorRotation.Quaternion());
}

void AThirdPersonDemoCharacter::Movecharacter(const float DeltaSeconds)
{
	if (bIsHanging || (bIsInCover && !bIsAiming)) return;

	// Get movement vector from inputs
	const float ForwardValue = GetInputAxisValue("MoveForward");
	const float RightValue = GetInputAxisValue("MoveRight");
	FVector MoveDirection = FVector(ForwardValue, RightValue, 0.f);

	GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("%f, %f"), ForwardValue, RightValue));

	// Get movement speed, clamp it if player is aiming when walking
	float MoveMagnitude = MoveDirection.Size();
	if (bIsAiming && !GetCharacterMovement()->IsFalling()) FMath::Clamp(MoveMagnitude, 0.f, MaxAimMoveRate);
	
	// Rotate movement direction by controller yaw to get world direction, then normalise magnitude to 1
	MoveDirection = UKismetMathLibrary::RotateAngleAxis(MoveDirection, GetControlRotation().Yaw, FVector::UpVector);
	MoveDirection.Normalize();

	AddMovementInput(MoveDirection, MoveMagnitude);
		
	// If already popping out from cover, exit cover
	if (bIsInCover && bIsAiming) ExitCover();

	// Keep character facing front when walking while aiming
	if (!bIsAiming) return;
	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Yaw = GetControlRotation().Yaw;
	SetActorRotation(ActorRotation.Quaternion());
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

void AThirdPersonDemoCharacter::TryUIHang()
{
	if (GetCharacterMovement()->IsFalling()) return;

	bool bCanShowHangUI = !bIsHanging && TraceUpClimb() && TraceForwardClimb() && UKismetMathLibrary::InRange_FloatFloat(TraceUpClimbResult.Location.Z - GetActorLocation().Z, ClimbUpMinDistance, ClimbUpMaxDistance + MaxJumpHeight);

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

void AThirdPersonDemoCharacter::TryHang()
{
	if (!(GetCharacterMovement()->IsFalling()) || bIsHanging || !TraceUpClimb() || !TraceForwardClimb()) return;

	if (!UKismetMathLibrary::InRange_FloatFloat(TraceUpClimbResult.Location.Z - GetActorLocation().Z, ClimbUpMinDistance, ClimbUpMaxDistance)) return;

	bIsHanging = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	PlayAnimMontage(ClimbMontage, 0.f);

	const FVector WallNormal = TraceForwardClimbResult.Normal;
	FVector HangLocation = TraceForwardClimbResult.Location + WallNormal * HangHorizontalOffset;
	HangLocation.Z = TraceUpClimbResult.Location.Z - HangVerticalOffset;
	const FRotator HangRotation = UKismetMathLibrary::MakeRotFromX(WallNormal * -1);
	FLatentActionInfo LatentActionInfo;
	LatentActionInfo.CallbackTarget = this;

	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), HangLocation, HangRotation, true, true, 0.2f, true, EMoveComponentAction::Move, LatentActionInfo);

	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Forward hit: %f, %f, %f"), TraceClimbForwardResult.Location.X, TraceClimbForwardResult.Location.Y, TraceClimbForwardResult.Location.Z));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Up hit: %f, %f, %f"), TraceClimbUpResult.Location.X, TraceClimbUpResult.Location.Y, TraceClimbUpResult.Location.Z));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Hang Location: %f, %f, %f"), HangLocation.X, HangLocation.Y, HangLocation.Z));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Hang Rotation: %f, %f, %f"), HangRotation.Pitch, HangRotation.Yaw, HangRotation.Roll));

}

void AThirdPersonDemoCharacter::TryClimbUp()
{
	if (bIsClimbing || !bIsHanging) return;

	const float AnimDuration = PlayAnimMontage(ClimbMontage);
	FTimerHandle ClimbUpTimerHandle;
	GetWorldTimerManager().SetTimer(ClimbUpTimerHandle, this, &AThirdPersonDemoCharacter::OnClimbUpFinished, AnimDuration - ClimbMontage->BlendOutTriggerTime);
	bIsClimbing = true;
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Moving in %f"), AnimDuration - ClimbMontage->BlendOutTriggerTime));
}

void AThirdPersonDemoCharacter::OnClimbUpFinished()
{
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Now")));
	FLatentActionInfo LatentActionInfo;
	LatentActionInfo.CallbackTarget = this;
	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), GetActorLocation() + GetActorForwardVector() * 30, GetActorRotation(), true, true, 0.25f, true, EMoveComponentAction::Move, LatentActionInfo);

	bIsHanging = false;
	bIsClimbing = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AThirdPersonDemoCharacter::TryDropDown()
{
	if (!bIsHanging) return;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	StopAnimMontage(ClimbMontage);

	bIsHanging = false;
}

bool AThirdPersonDemoCharacter::TraceUpClimb()
{
	const FVector TraceEnd = GetActorLocation() + GetActorForwardVector() * ClimbForwardDistance + FVector::UpVector * ClimbUpMinDistance;
	const FVector TraceStart = TraceEnd + FVector::UpVector * (ClimbUpMaxDistance + MaxJumpHeight);
	//const FRotator TraceRotation(0.f, 0.f, 0.f);
	const EDrawDebugTrace::Type DrawMode = bDrawDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	return UKismetSystemLibrary::LineTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceUpClimbResult, true);
	//return UKismetSystemLibrary::BoxTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceHalfSize, TraceRotation, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceUpClimbResult, true);
}

bool AThirdPersonDemoCharacter::TraceForwardClimb()
{
	const FVector TraceStart = FVector(GetActorLocation().X, GetActorLocation().Y, TraceUpClimbResult.Location.Z - TraceOffset * 2);
	const FVector TraceEnd = TraceStart + GetActorForwardVector() * ClimbForwardDistance;
	//const FQuat TraceRotation = GetActorQuat();
	const EDrawDebugTrace::Type DrawMode = bDrawDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	return UKismetSystemLibrary::LineTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceForwardClimbResult, true);
	//return UKismetSystemLibrary::SphereTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceShapeRadius, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceForwardClimbResult, true);
}

void AThirdPersonDemoCharacter::StartAim()
{
	if (bIsInCover)
	{
		FVector ActorLocation = GetActorLocation();
		if (bIsTallCover) ActorLocation += UKismetMathLibrary::RotateAngleAxis(GetActorForwardVector(), bIsRightCover ? -90.f : 90.f, FVector::UpVector) * CoverAimYOffset;

		FLatentActionInfo LatentActionInfo;
		LatentActionInfo.CallbackTarget = this;

		UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), ActorLocation, CoverRotator + FRotator(0.f, 180.f, 0.f), true, true, 0.2f, true, EMoveComponentAction::Move, LatentActionInfo);
	}
	
	//bUseControllerRotationYaw = true;
	bIsAiming = true;
	RecalculateTargetCameraOffset();
}

void AThirdPersonDemoCharacter::EndAim()
{
	if (bIsInCover)
	{
		FLatentActionInfo LatentActionInfo;
		LatentActionInfo.CallbackTarget = this;

		UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), CoverLocation, CoverRotator, true, true, 0.2f, true, EMoveComponentAction::Move, LatentActionInfo);
	}

	bUseControllerRotationYaw = false;
	bIsAiming = false;
	RecalculateTargetCameraOffset();
}

void AThirdPersonDemoCharacter::ToggleCover()
{
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Walking) return;

	if (!bIsInCover) TryEnterCover();
	else ExitCover();
}

void AThirdPersonDemoCharacter::TryEnterCover()
{
	if (bIsInCover || !TraceForwardCover()) return;

	// Get the angle of the line trace against the wall to determine if peeking out of the left or right cover
	const FVector2D CharacterForwardVector = FVector2D(GetActorForwardVector().X, GetActorForwardVector().Y);
	const FVector2D WallNormal = FVector2D(TraceForwardCoverResult.Normal.X, TraceForwardCoverResult.Normal.Y);
	const float DotProduct = FVector2D::DotProduct(CharacterForwardVector, WallNormal);
	const float Determinant = CharacterForwardVector.X * WallNormal.Y - CharacterForwardVector.Y * WallNormal.X;

	bIsRightCover = atan2(Determinant, DotProduct) > 0;
	if (bIsTallCover && !TraceSideCover())
	{
		bIsRightCover = !bIsRightCover;
		if (!TraceSideCover()) return;
	}

	/*
	const float AnimDuration = PlayAnimMontage(EnterCoverRightMontage);
	FTimerHandle CoverTimerHandle;
	GetWorldTimerManager().SetTimer(CoverTimerHandle, this, &AThirdPersonDemoCharacter::OnEnterCoverFinished, AnimDuration - EnterCoverRightMontage->BlendOutTriggerTime);
	*/

	bIsInCover = true;
	bIsAiming = false;
	RecalculateTargetCameraOffset();
	OnEnterCoverFinished();
}
 
void AThirdPersonDemoCharacter::OnEnterCoverFinished()
{
	CoverLocation = bIsTallCover ? 
		TraceSideCoverResult.Location - TraceSideCoverResult.Normal * CoverSideOffset + TraceForwardCoverResult.Normal * (CoverForwardOffset + TraceOffset) + FVector::DownVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere():
		TraceForwardCoverResult.Location + TraceForwardCoverResult.Normal * CoverForwardOffset;
	CoverRotator = TraceForwardCoverResult.Normal.Rotation();
	FLatentActionInfo LatentActionInfo;
	LatentActionInfo.CallbackTarget = this;

	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, bIsTallCover ? TEXT("TALL COVER") : TEXT("SHORT COVER"));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, bIsRightCover ? TEXT("RIGHT COVER") : TEXT("LEFT COVER"));

	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), CoverLocation, CoverRotator, true, true, 0.2f, true, EMoveComponentAction::Move, LatentActionInfo);
}

void AThirdPersonDemoCharacter::ExitCover()
{
	bIsInCover = false;
	RecalculateTargetCameraOffset();
}

bool AThirdPersonDemoCharacter::TraceForwardCover()
{
	FVector TraceStart = GetActorLocation() + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere();
	FVector TraceEnd = TraceStart + GetActorForwardVector() * CoverForwardDistance;
	const EDrawDebugTrace::Type DrawMode = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	if (UKismetSystemLibrary::LineTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceForwardCoverResult, true))
	{
		bIsTallCover = true;
		return true;
	}

	bIsTallCover = false;
	TraceStart = GetActorLocation();
	TraceEnd = TraceStart + GetActorForwardVector() * CoverForwardDistance;
	return UKismetSystemLibrary::LineTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceForwardCoverResult, true);
}

bool AThirdPersonDemoCharacter::TraceSideCover()
{
	const FVector TraceEnd = TraceForwardCoverResult.Location - TraceForwardCoverResult.Normal * TraceOffset;
	const FVector TraceStart = TraceEnd + UKismetMathLibrary::RotateAngleAxis(TraceForwardCoverResult.Normal, bIsRightCover ? -90.f : 90.f, FVector::UpVector) * CoverSideDistance;
	const EDrawDebugTrace::Type DrawMode = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	return UKismetSystemLibrary::LineTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceSideCoverResult, true);
}
