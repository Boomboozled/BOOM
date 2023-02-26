// Fill out your copyright notice in the Description page of Project Settings.


#include "BOOMPickUpComponent.h"
#include "BOOM/BOOMCharacter.h"
// Sets default values for this component's properties
UBOOMPickUpComponent::UBOOMPickUpComponent()
{
	//SphereRadius = PickUpRadius Should be based around weapon size maybe? Arbitrary value for now;
	SphereRadius = 50;
}

void UBOOMPickUpComponent::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ABOOMCharacter* TargetCharacter = Cast<ABOOMCharacter>(OtherActor))
	{
		TargetCharacter->bIsOverlappingWeapon = false;
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0F, FColor::Orange, "character no longer overlapping weapon");
	}


}

// Called when the game starts
void UBOOMPickUpComponent::BeginPlay()
{
	Super::BeginPlay();
	OnComponentBeginOverlap.AddDynamic(this, &UBOOMPickUpComponent::OnSphereBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UBOOMPickUpComponent::OnSphereEndOverlap);


}

void UBOOMPickUpComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	if (ABOOMCharacter* TargetCharacter = Cast<ABOOMCharacter>(OtherActor))
	{
		if (!TargetCharacter->bIsOverlappingWeapon)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.0F, FColor::Green,"Player is overlapping weapon");
			TargetCharacter->bIsOverlappingWeapon = true;
			OnPickUp.Broadcast(TargetCharacter);
		}
	}

}


