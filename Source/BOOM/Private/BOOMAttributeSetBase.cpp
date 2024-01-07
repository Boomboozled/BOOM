// Project BOOM


#include "BOOMAttributeSetBase.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

#include "BOOM/BOOMCharacter.h"

UBOOMAttributeSetBase::UBOOMAttributeSetBase()
{
}

void UBOOMAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME_CONDITION_NOTIFY(UBOOMAttributeSetBase, Health, COND_None, REPNOTIFY_Always);

}

void UBOOMAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.F, GetMaxHealth());
	}
	else if (Attribute == GetShieldStrengthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.F, GetMaxShieldStrength());
	}

	UE_LOG(LogTemp, Warning, TEXT("UBOOMAttributeSetBase::PreAttributeChange. New value is: %f"), NewValue)

}

void UBOOMAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	UE_LOG(LogTemp, Warning, TEXT("UBOOMAttributeSetBase::PostGameplayEffectExecute"));
	Super::PostGameplayEffectExecute(Data);

	//A programming context can be defined as all the relevant information that a developer needs to complete a task.
	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* Source = Context.GetOriginalInstigatorAbilitySystemComponent();

	AActor* TargetActor = nullptr;
	AController* TargetController = nullptr;
	ABOOMCharacter* TargetCharacter = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		TargetCharacter = Cast<ABOOMCharacter>(TargetActor);
	}
	
	//AActor* SourceActor = nullptr;
	//AController* SourceController = nullptr;
	//
	//if (Source && Source->AbilityActorInfo.IsValid() && Source->AbilityActorInfo->AvatarActor.IsValid())
	//{
	//	SourceActor = Source->AbilityActorInfo->AvatarActor.Get();
	//	SourceController = Source->AbilityActorInfo->PlayerController.Get();
	//}

	/*
	Instead of directly affecting attribute like shields and health. Have a meta attribute (Damage). When damage is the attribute being applied, we can now check shields and health.	
	*/

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		UE_LOG(LogTemp, Warning, TEXT("DamageAttr"));

		float LocalDamage = GetDamage();
		SetDamage(0.F);
		UE_LOG(LogTemp, Warning, TEXT("LocalDmg: %f"), LocalDamage)
		float LocalHealth = GetHealth();

			//need to convert remaining damage to health damage.
		if (LocalDamage > 0.0F)
		{
			UE_LOG(LogTemp, Warning, TEXT("Damage value is: %f"), LocalDamage)

			float LocalShieldStrength = GetShieldStrength();

			float RemainingDamage = 0.0f;
			if (LocalShieldStrength > 0.0F)
			{
				float ShieldDamageTaken = LocalShieldStrength - LocalDamage;
				SetShieldStrength(ShieldDamageTaken);
				//@TODO - for shield/health damage differences, figure out proper way within framework to control remainder damage.
				if (FMath::IsNegative(ShieldDamageTaken))
				{
					RemainingDamage = FMath::Abs(ShieldDamageTaken);
					SetHealth(LocalHealth - RemainingDamage);
				}

			}
			else
			{
				SetHealth(LocalHealth - LocalDamage); 
			}
			UE_LOG(LogTemp, Warning, TEXT("Shield strength: %f"), GetShieldStrength())
			UE_LOG(LogTemp, Warning, TEXT("Health: %f"), GetHealth())

			if (TargetCharacter && !TargetCharacter->IsAlive())
			{


			}


		}
	}

}


//void UBOOMAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
//{
//	GAMEPLAYATTRIBUTE_REPNOTIFY(UBOOMAttributeSetBase, Health, OldHealth);
//
//}
