// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonDemoCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
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
	PlayerInputComponent->BindAxis("MoveForward", this, &AThirdPersonDemoCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AThirdPersonDemoCharacter::MoveRight);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("ClimbUp", IE_Pressed, this, &AThirdPersonDemoCharacter::TryClimbUp);
	PlayerInputComponent->BindAction("DropDown", IE_Pressed, this, &AThirdPersonDemoCharacter::TryDropDown);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AThirdPersonDemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AThirdPersonDemoCharacter::LookUpAtRate);
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
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonDemoCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonDemoCharacter::MoveForward(float Value)
{
	if (bIsHanging) return;

	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AThirdPersonDemoCharacter::MoveRight(float Value)
{
	if (bIsHanging) return;

	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void AThirdPersonDemoCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TraceForwardClimb();
	TraceUpClimb();
	TryHang();

	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("POS: %f, %f, %f"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z));
	//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("%s"), GetCharacterMovement()->IsFalling() ? TEXT("true") : TEXT("false")));
	//GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Forward Vector: %f, %f, %f"), GetActorForwardVector().X, GetActorForwardVector().Y, GetActorForwardVector().Z));
}

void AThirdPersonDemoCharacter::TryHang()
{
	if (!(GetCharacterMovement()->IsFalling()) || bIsHanging || !TraceForwardClimb() || !TraceUpClimb()) return;

	bIsHanging = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	PlayAnimMontage(ClimbMontage, 0.f);

	const FVector WallNormal = TraceClimbForwardResult.Normal;
	FVector HangLocation = TraceClimbForwardResult.Location + WallNormal * HangHorizontalOffset;
	HangLocation.Z = TraceClimbUpResult.Location.Z - HangVerticalOffset;
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
	GetWorldTimerManager().SetTimer(ClimbUpTimerHandle, this, &AThirdPersonDemoCharacter::OnClimbUpFinished, AnimDuration - ClimbMontage->BlendOutTriggerTime, false);
	bIsClimbing = true;
	GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Moving in %f"), AnimDuration - ClimbMontage->BlendOutTriggerTime));
}

void AThirdPersonDemoCharacter::OnClimbUpFinished()
{
	GEngine->AddOnScreenDebugMessage(-1, 999.f, FColor::Red, FString::Printf(TEXT("Now")));
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
	const FVector TraceEnd = GetActorLocation() + GetActorForwardVector() * ClimbForwardDistance;
	const FVector TraceStart = TraceEnd + FVector::UpVector * 500.f;
	const FVector TraceHalfSize = FVector(TraceShapeRadius, TraceShapeRadius, TraceShapeRadius);
	const FRotator TraceRotation = FRotator(0.f, 0.f, 0.f);
	const EDrawDebugTrace::Type DrawMode = bDrawDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	return UKismetSystemLibrary::BoxTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceHalfSize, TraceRotation, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceClimbUpResult, true)
		&& (UKismetMathLibrary::InRange_FloatFloat(TraceClimbUpResult.Location.Z - GetActorLocation().Z, ClimbUpMinDistance, ClimbUpMaxDistance));
}

bool AThirdPersonDemoCharacter::TraceForwardClimb()
{
	const FVector TraceStart = FVector(GetActorLocation().X, GetActorLocation().Y, TraceClimbUpResult.Location.Z - TraceShapeRadius * 2);
	const FVector TraceEnd = TraceStart + GetActorForwardVector() * ClimbForwardDistance;
	const FQuat TraceRotation = GetActorQuat();
	const EDrawDebugTrace::Type DrawMode = bDrawDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

	return UKismetSystemLibrary::SphereTraceSingle(GetWorld(), TraceStart, TraceEnd, TraceShapeRadius, TraceTypeQuery_MAX, false, { this }, DrawMode, TraceClimbForwardResult, true);
}
