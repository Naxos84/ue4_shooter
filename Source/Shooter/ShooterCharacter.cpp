// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter():
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom (puuls in towards character if there is a collision
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.f; //camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; //Rotate the arm based on the controller

	// Create follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); //Attach camera to end of cameraboom
	FollowCamera->bUsePawnControlRotation = false; //Camera does not rotate relative to arm

	// don't rotate when the controller rotates. Let the controller only affect the camera
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	//Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; //character moves in the direction of input
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f); // at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = .2f;
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("BeginPlay() called!"));

	int myInt{ 42 };
	float myFloat{ 3.14159f };
	double myDouble{ 0.123 };
	char myChar{ 'C' };
	wchar_t myWideChar{ L'C' };
	bool myBoolean{ true };
	UE_LOG(LogTemp, Warning, TEXT("myInt: %d and myFloat: %f and myDouble: %lf and myChar: %c and myWideChar: %lc and myBoolean: %d"), myInt, myFloat, myDouble, myChar, myWideChar, myBoolean);

	FString myString{ TEXT("My String!!") };
	UE_LOG(LogTemp, Warning, TEXT("FString: %s"), *myString)

		UE_LOG(LogTemp, Warning, TEXT("Name of instance: %s"), *GetName());

	CameraBoom->SocketOffset = CameraOffset;// FVector(0.f, 50.f, 50.f);
}

void AShooterCharacter::MoveForward(float Value)
{
	if (Controller != nullptr && Value != 0.0f)
	{
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };

		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Controller != nullptr && Value != 0.0f)
	{
		const FRotator Rotation{ Controller->GetControlRotation() };
		const FRotator YawRotation{ 0, Rotation.Yaw, 0 };

		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };

		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::TurnAtRate(float Rate)
{
	//Calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	//Calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::FireWeapon()
{
	UE_LOG(LogTemp, Warning, TEXT("Fire Weapon"));
	if (FireSound) {
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("No FireSound defined"));
	}

	const USkeletalMeshSocket* BarrelSocket = GetMesh()->GetSocketByName("barrel_socket");
	if (BarrelSocket) {
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetMesh());

		if (MuzzleFlash) {
			UGameplayStatics::SpawnEmitterAtLocation(this->GetWorld(), MuzzleFlash, SocketTransform);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("No MuzzleFlash defined"));
		}

		FVector BeamEndPoint;
		if (this->GetBeamEndLocation(SocketTransform.GetLocation(), BeamEndPoint)) {

			if (ImpactParticles) {
				UGameplayStatics::SpawnEmitterAtLocation(this->GetWorld(), ImpactParticles, BeamEndPoint);
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("No bullethit particles defined"));
			}

			if (BeamParticles) {
				UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(this->GetWorld(), BeamParticles, SocketTransform);
				if (Beam) {
					Beam->SetVectorParameter(FName("Target"), BeamEndPoint);
				}
				else UE_LOG(LogTemp, Warning, TEXT("Could not spawn beam"));
			}
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("No Barrel socket defined"));
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HipFireMontage) {
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	//Get current size of viewport
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport) {
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	//Get screen based location of crosshair
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	CrosshairLocation.Y -= 50.f;

	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	//get world position and direction of crosshair
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0), CrosshairLocation, CrosshairWorldPosition, CrosshairWorldDirection); //uses "first" player

	//was deprojection successful?
	if (bScreenToWorld) {
		FHitResult ScreenTraceHit;
		const FVector Start{ CrosshairWorldPosition };
		const FVector End{ CrosshairWorldPosition + CrosshairWorldDirection * 50'000 };
		OutBeamLocation = End ;

		//trace outward from crosshair world location
		this->GetWorld()->LineTraceSingleByChannel(ScreenTraceHit, Start, End, ECollisionChannel::ECC_Visibility);

		//if something "visible" was hit
		if (ScreenTraceHit.bBlockingHit) {
			OutBeamLocation = ScreenTraceHit.Location;
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Nothing hit with Gunshot"));
		}

		//perform a second trace this time from gun barrel
		FHitResult WeaponTraceHit;
		const FVector WeaponTraceStart{ MuzzleSocketLocation };
		const FVector WeaponTraceEnd{ OutBeamLocation };

		this->GetWorld()->LineTraceSingleByChannel(WeaponTraceHit, WeaponTraceStart, WeaponTraceEnd, ECollisionChannel::ECC_Visibility);
		//object between barrel and BeamEndPoint
		if (WeaponTraceHit.bBlockingHit) {
			OutBeamLocation = WeaponTraceHit.Location;
		}

		return true;
	}
	return false;
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	//Binds AxisMappings from ProjectSettings to methods in this class
	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	//Binds ActionMappings from ProjectSettings to methods in this class
	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &AShooterCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Released, this, &AShooterCharacter::StopJumping);
	PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Released, this, &AShooterCharacter::FireWeapon);

}

