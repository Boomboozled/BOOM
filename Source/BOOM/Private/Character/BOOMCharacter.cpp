// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/BOOMCharacter.h"
#include "BOOM/BOOMProjectile.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Weapons/BOOMWeapon.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/InteractableInterface.h"
#include "UI/BOOMPlayerHUD.h"
#include "Containers/Array.h"
#include "Math/NumericLimits.h"
#include "AI/BOOMAIController.h"
#include "Character/BOOMHealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/BOOMCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Character/BOOMAttributeSetBase.h"

//////////////////////////////////////////////////////////////////////////
// ABOOMCharacter

ABOOMCharacter::ABOOMCharacter()
{	
	PrimaryActorTick.bCanEverTick = true;
	// Character doesnt have a rifle at start
	bHasRifle = false;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>("FirstPersonCamera");
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	
	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>("CharacterMesh1P");
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	GetMesh()->CastShadow = true;
	GetMesh()->bCastDynamicShadow = true;

	GetMesh()->SetOnlyOwnerSee(true);
	//Debating on own 3P mesh, or just using the mesh class as the 3p mesh.
	//Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>("CharacterMesh3P");
	//Mesh3P->SetOnlyOwnerSee(false);
	//Mesh3P->SetupAttachment(FirstPersonCameraComponent);
	//Mesh3P->bCastDynamicShadow = true;
	//Mesh3P->CastShadow = true;

	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	SocketNameGripPoint = "GripPoint";
	SocketNameHolsterPoint = "spine_01";
	SocketNameGripPoint3P = "hand_r";
	InteractionRange = 250.0F;
	Overlaps = 0;
	bGenerateOverlapEventsDuringLevelStreaming = true;
	CurrentWeaponSlot = 0;
	MaxWeaponsEquipped = 2;
	bIsPendingFiring = false;

	HealthComponent = CreateDefaultSubobject<UBOOMHealthComponent>("HealthComponent");

	bIsFocalLengthScalingEnabled = false;

	//b crouch maintains place location could be used for crouch jumping
	
	BOOMCharacterMovementComp = Cast<UBOOMCharacterMovementComponent>(GetCharacterMovement());

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);

	AttributeSetBase = CreateDefaultSubobject<UBOOMAttributeSetBase>("AttributeSetBase");
	AttributeSetBase->InitHealth(100.F);
	AttributeSetBase->InitMaxHealth(100.F);
	AttributeSetBase->InitShieldStrength(100.F);
	AttributeSetBase->InitMaxShieldStrength(100.F);

	//Can always get handles to delegates if needed.

	//working on figuring out why this doesn't work...
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSetBase->GetHealthAttribute()).AddUObject(this, &ABOOMCharacter::HandleHealthChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSetBase->GetShieldStrengthAttribute()).AddUObject(this, &ABOOMCharacter::HandleShieldStrengthChanged);
	ShieldRechargeInterpSeconds = 0.01f;
	ShieldFullRechargeDurationSeconds = 3.f;
	ShieldRechargeDelaySeconds = 5.F;
}

void ABOOMCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ABOOMCharacter::OnCharacterBeginOverlap);
	GetCapsuleComponent()->OnComponentEndOverlap.AddDynamic(this, &ABOOMCharacter::OnCharacterEndOverlap);

	/*
	Actors already overlapping will not cause a begin overlap event, therefore need to check size of overlapped actors on begin play.
	*/
	GetOverlappingActors(OverlappedActors);
	Overlaps = OverlappedActors.Num();
	
	//For Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		PlayerHUD = CreateWidget<UBOOMPlayerHUD>(PlayerController, PlayerHUDClass);
		check(PlayerHUD);
		PlayerHUD->AddToPlayerScreen();

		FTimerHandle InteractionHandle;
		GetWorld()->GetTimerManager().SetTimer(InteractionHandle, this, &ABOOMCharacter::CheckPlayerLook, 0.1F, true);
	}
	//@TODO Can cause game to crash when a PlayerHUD is expected and we have non-player characters
	SpawnWeapons();

}

void ABOOMCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);


}

void ABOOMCharacter::OnCharacterBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Overlaps++;
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 1.0F, FColor::Red, FString::FromInt(Overlaps));

}	


void ABOOMCharacter::OnCharacterEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Overlaps--;
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 1.0F, FColor::Blue, FString::FromInt(Overlaps));

}

//////////////////////////////////////////////////////////////////////////// Input
void ABOOMCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABOOMCharacter::StartCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ABOOMCharacter::EndCrouch); 

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABOOMCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABOOMCharacter::Look);

		//Cyclical Weapon Swapping
		EnhancedInputComponent->BindAction(WeaponSwapAction, ETriggerEvent::Started, this, &ABOOMCharacter::SwapWeapon);

		//Weapon Pickup
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ABOOMCharacter::Interact);

		//Fire Weapon
		//@TODO - Possibly let weapons bind their own firing input responses
		//EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ABOOMCharacter::StartFire);
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Started, this, &ABOOMCharacter::Zoom);

		//EnhancedInputComponent->BindAction(StopFireAction, ETriggerEvent::Completed, this, &ABOOMCharacter::StopFire);

		//Reload Weapon
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABOOMCharacter::Reload);

	}
}

void ABOOMCharacter::Reload()
{
	if (HasNoWeapons())
	{
		return;
	}

	Weapon = Weapons[CurrentWeaponSlot];
	if (Weapon != nullptr)
	{
		Weapon->HandleReloadInput();
	}
}

void ABOOMCharacter::Fire()
{

}

void ABOOMCharacter::StopFire()
{
	bIsPendingFiring = false;
	if (Weapons.IsValidIndex(CurrentWeaponSlot))
	{
		if (Weapons[CurrentWeaponSlot] != nullptr)
		{
			Weapons[CurrentWeaponSlot]->HandleStopFireInput();
		}
	}
}

void ABOOMCharacter::DropCurrentWeapon()
{
	if (Weapons.IsValidIndex(CurrentWeaponSlot))
	{
		if (Weapons[CurrentWeaponSlot] != nullptr)
		{
			Weapons[CurrentWeaponSlot]->HandleBeingDropped();
		}
	}
}

UBOOMPlayerHUD* ABOOMCharacter::GetPlayerHUD()
{
	return PlayerHUD;
}


AActor* ABOOMCharacter::GetNearestInteractable()
{
	float MinDistance = TNumericLimits<float>::Max();
	AActor* NearestActor = nullptr;
	GetOverlappingActors(OverlappedActors);
	for (AActor* OverlappedActor : OverlappedActors)
	{
		IInteractableInterface* InteractableObject = Cast<IInteractableInterface>(OverlappedActor);

		if (InteractableObject)
		{
			float DistanceToPlayer = FVector::Distance(this->GetActorLocation(), OverlappedActor->GetActorLocation());
			if (DistanceToPlayer <= MinDistance)
			{
				MinDistance = DistanceToPlayer;
				NearestActor = OverlappedActor;
			}
		}
	}
	return NearestActor;
}


void ABOOMCharacter::CheckPlayerLook()
{
	/*
	Player-look functionality for interacting with objects. Look interactivity is prioritized over proximity pickup.
	*/
	APlayerController* PlayerController = Cast<APlayerController>(this->GetController());
	if (PlayerController)
	{
		FRotator CameraRotation;
		FVector CameraLocation;
		CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();

		FVector Start = CameraLocation;
		FVector End = Start + (CameraRotation.Vector() * InteractionRange);
		FHitResult HitResult;
		FCollisionQueryParams TraceParams;

		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_GameTraceChannel4, TraceParams);
		
		IInteractableInterface* InteractableObject;
		if (bHit)
		{
			if (HighlightedActor == HitResult.GetActor())
			{
				return;
			}
			HighlightedActor = HitResult.GetActor();

			InteractableObject = Cast<IInteractableInterface>(HighlightedActor);
			if (InteractableObject)
			{
				InteractableObject->OnInteractionRangeEntered(this);
			}
		}
		else
		{
			InteractableObject = Cast<IInteractableInterface>(HighlightedActor);
			HighlightedActor = nullptr;
			if (InteractableObject)
			{
				InteractableObject->OnInteractionRangeExited(this);
			}
			
			if(Overlaps > 0)
			{
				HighlightedActor = this->GetNearestInteractable();
				if (HighlightedActor)
				{
					InteractableObject = Cast<IInteractableInterface>(HighlightedActor);

					InteractableObject->OnInteractionRangeEntered(this);
				}			
			}
		}
	}
}

//@TODO: Revamp later when other inventory features are implemented.
void ABOOMCharacter::SpawnWeapons()
{
	if (WeaponsToSpawn.Num() > MaxWeaponsEquipped)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 1.0F, FColor::Red, "Too many weapons added to character");
	}
	
	for (TSubclassOf<ABOOMWeapon> WeaponClass : WeaponsToSpawn)
	{
		if(!WeaponClass)
		{
			continue;
		}

		ABOOMWeapon* SpawnedWeapon = GetWorld()->SpawnActor<ABOOMWeapon>(WeaponClass, GetActorLocation(), GetActorRotation());
		check(SpawnedWeapon)
		if (Weapons.Num() > MaxWeaponsEquipped)
		{
			break;
		}
		EquipWeapon(SpawnedWeapon);
	}

}

void ABOOMCharacter::EquipWeapon(ABOOMWeapon* TargetWeapon)
{
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);

	TargetWeapon->Character = this;


	TargetWeapon->DisableCollision();
	if (HasNoWeapons())
	{
		Weapons.Add(TargetWeapon);


		TargetWeapon->GotoStateEquipping();

		//jank placeholder
		if (GetMesh1P())
		{
			TargetWeapon->AttachToComponent(GetMesh1P(), AttachmentRules, SocketNameGripPoint);
		}

		
		if (PlayerHUD)
		{
			GetPlayerHUD()->GetWeaponInformationElement()->SetWeaponNameText(Weapons[CurrentWeaponSlot]->Name);
			GetPlayerHUD()->GetWeaponInformationElement()->SetCurrentAmmoText(Weapons[CurrentWeaponSlot]->CurrentAmmo);
			GetPlayerHUD()->GetWeaponInformationElement()->SetReserveAmmoText(Weapons[CurrentWeaponSlot]->CurrentAmmoReserves);
		}

	}
	else if (HasEmptyWeaponSlots())
	{
		Weapons.Add(TargetWeapon);

		TargetWeapon->GotoStateInactive();
		TargetWeapon->AttachToComponent(GetMesh1P(), AttachmentRules, SocketNameHolsterPoint);

	}
	else
	{
		/*
		@TODO:  Avoid or Prevent - If collision is enabled, the character can pick up a weapon that is in their hand and also attempt to drop it, which causes the
		weapon's character reference to be assigned to the same character, and then nullified, causing seg faults.
		*/
		DropCurrentWeapon();
		TargetWeapon->GotoStateEquipping();
		Weapons[CurrentWeaponSlot] = TargetWeapon;
		TargetWeapon->AttachToComponent(GetMesh1P(), AttachmentRules, SocketNameGripPoint);
		if (PlayerHUD)
		{
			GetPlayerHUD()->GetWeaponInformationElement()->SetWeaponNameText(Weapons[CurrentWeaponSlot]->Name);
			GetPlayerHUD()->GetWeaponInformationElement()->SetCurrentAmmoText(Weapons[CurrentWeaponSlot]->CurrentAmmo);
			GetPlayerHUD()->GetWeaponInformationElement()->SetReserveAmmoText(Weapons[CurrentWeaponSlot]->CurrentAmmoReserves);
		}
	}
	SetHasRifle(true);
}

bool ABOOMCharacter::HasNoWeapons()
{
	return Weapons.Num() == 0;
}

bool ABOOMCharacter::HasEmptyWeaponSlots()
{
	return Weapons.Num() != MaxWeaponsEquipped;
}

void ABOOMCharacter::OnDeath()
{
	if (Controller)
	{
		Controller->UnPossess();
	}
	//todo - specify function
	//otherwise could cause bug hard to find
	OnTakePointDamage.RemoveAll(this);

	OnTakeAnyDamage.RemoveAll(this);
	ThrowInventory();
	if (PlayerHUD)
	{
		PlayerHUD->RemoveFromParent();
	}
	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}
}

void ABOOMCharacter::ThrowInventory()
{
	for (ABOOMWeapon* TWeapon : Weapons) //jank code 
	{
		TWeapon->HandleBeingDropped();
		//Can isolate death physics behaviors or have dropping behavior that is shared on death.
	}
	Weapons.Empty();
}

void ABOOMCharacter::InterpShieldRegen()
{
	//UE_LOG(LogTemp, Warning, TEXT("RegenInterval"))

	float ShieldStrengthFloat = AttributeSetBase->GetShieldStrength();

	float ShieldMax = AttributeSetBase->GetMaxShieldStrength();
	//UE_LOG(LogTemp, Warning, TEXT("Shield Max: %f"), ShieldMax)

	

	float UpdateVal = ShieldStrengthFloat + ((ShieldMax /ShieldFullRechargeDurationSeconds) * ShieldRechargeInterpSeconds);

	//UE_LOG(LogTemp, Warning, TEXT("Update Val: %f"), UpdateVal)

	AttributeSetBase->SetShieldStrength(UpdateVal);
}


void ABOOMCharacter::RegenerateShields()
{
	//local for testing
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_ShieldRecharge, this, &ABOOMCharacter::InterpShieldRegen, ShieldRechargeInterpSeconds, true);
}

void ABOOMCharacter::HandleShieldStrengthChanged(const FOnAttributeChangeData& Data)
{
 
	if (FMath::IsNearlyEqual(Data.OldValue, Data.NewValue))
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ShieldRecharge);
	}

	if (Data.OldValue > Data.NewValue)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ShieldRecharge);

		GetWorld()->GetTimerManager().SetTimer(TimerHandle_ShieldRechargeDelay, this, &ABOOMCharacter::RegenerateShields, ShieldRechargeDelaySeconds);
	}
	
	if (PlayerHUD)
	{
		//move to shield component
		PlayerHUD->GetHealthInformationElement()->SetShieldBar(Data.NewValue / AttributeSetBase->GetMaxShieldStrength());
	}

	

}


void ABOOMCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.OldValue > Data.NewValue)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ShieldRecharge);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_ShieldRechargeDelay, this, &ABOOMCharacter::RegenerateShields, ShieldRechargeDelaySeconds);
	}

	if (PlayerHUD)
	{
		//move to health components	
		PlayerHUD->GetHealthInformationElement()->SetHealthBar(Data.NewValue/AttributeSetBase->GetMaxHealth());
	}


	if (!IsAlive())
	{
		OnDeath();
		return;
	}
}


void ABOOMCharacter::Zoom()
{
	if (Weapons.IsValidIndex(CurrentWeaponSlot) && Weapons[CurrentWeaponSlot] != nullptr)
	{
		Weapons[CurrentWeaponSlot]->Zoom();
	}

}

float ABOOMCharacter::GetHealth()
{
	if (IsValid(AttributeSetBase))
	{
		return AttributeSetBase->GetHealth();
	}
	
	return 0.0F;
}

bool ABOOMCharacter::IsAlive()
{
	return GetHealth() > 0.0F;
}


void ABOOMCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void ABOOMCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	
	if (Controller != nullptr)
	{
		//@TODO: Move to player settings and include save system

		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);

	}
}

void ABOOMCharacter::SwapWeapon(const FInputActionValue& Value)
{
	if (Weapons.Num() <= 1)
	{
		return;
	}
	//@TODO - switch to isValidIndex checks
	if (Weapons.IsValidIndex(CurrentWeaponSlot) && Weapons[CurrentWeaponSlot] != nullptr)
	{

		FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
		
		Weapons[CurrentWeaponSlot]->AttachToComponent(this->GetMesh1P(), AttachmentRules, SocketNameHolsterPoint);
		
		Weapons[CurrentWeaponSlot]->HandleUnequipping();

		//
		CurrentWeaponSlot++;
		CurrentWeaponSlot = (CurrentWeaponSlot % MaxWeaponsEquipped);

		if (Weapons.IsValidIndex(CurrentWeaponSlot) && Weapons[CurrentWeaponSlot] != nullptr) //maybe use isValidIndex instead
		{
			Weapons[CurrentWeaponSlot]->AttachToComponent(this->GetMesh1P(), AttachmentRules, SocketNameGripPoint);
			Weapons[CurrentWeaponSlot]->HandleEquipping();
			if (PlayerHUD)
			{
				GetPlayerHUD()->GetWeaponInformationElement()->SetWeaponNameText(Weapons[CurrentWeaponSlot]->Name);
				GetPlayerHUD()->GetWeaponInformationElement()->SetCurrentAmmoText(Weapons[CurrentWeaponSlot]->CurrentAmmo);
				GetPlayerHUD()->GetWeaponInformationElement()->SetReserveAmmoText(Weapons[CurrentWeaponSlot]->CurrentAmmoReserves);
			}
		}
	}
}

void ABOOMCharacter::StartCrouch(const FInputActionValue& Value)
{
	Crouch();
}

void ABOOMCharacter::EndCrouch(const FInputActionValue& Value)
{
	UnCrouch();
}

void ABOOMCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	Super::Jump();

}

//void ABOOMCharacter::StartFire(const FInputActionValue& Value)
void ABOOMCharacter::StartFire()
{
	bIsPendingFiring = true;
	if (HasNoWeapons())
	{
		return;
	}

	if (Weapons[CurrentWeaponSlot] != nullptr)
	{
		Weapons[CurrentWeaponSlot]->HandleFireInput();
	}
}

void ABOOMCharacter::Interact(const FInputActionValue& Value)
{
	IInteractableInterface* InteractableObject;
	if (HighlightedActor)
	{
		InteractableObject = Cast<IInteractableInterface>(HighlightedActor);
		if(InteractableObject)
		{
			HighlightedActor = nullptr;
			InteractableObject->Interact(this);
			InteractableObject->OnInteractionRangeExited(this);
		}
		this->GetNearestInteractable();
		return;
	}
}

void ABOOMCharacter::SetHasRifle(bool bNewHasRifle)
{
	bHasRifle = bNewHasRifle;
}

bool ABOOMCharacter::GetHasRifle()
{
	return bHasRifle;
}
