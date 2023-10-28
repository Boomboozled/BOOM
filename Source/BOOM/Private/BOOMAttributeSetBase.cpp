// Project BOOM


#include "BOOMAttributeSetBase.h"

UBOOMAttributeSetBase::UBOOMAttributeSetBase()
{
	InitHealth(100.F);
}

void UBOOMAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME_CONDITION_NOTIFY(UBOOMAttributeSetBase, Health, COND_None, REPNOTIFY_Always);

}

void UBOOMAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.F, GetMaxHealth());
	}
}


//void UBOOMAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
//{
//	GAMEPLAYATTRIBUTE_REPNOTIFY(UBOOMAttributeSetBase, Health, OldHealth);
//
//}