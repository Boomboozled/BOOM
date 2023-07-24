// Project BOOM

#pragma once

#include "CoreMinimal.h"
#include "BOOM/BOOMCharacter.h"
#include "BOOMAICharacter.generated.h"

/**
 * 
 */
UCLASS()
class BOOM_API ABOOMAICharacter : public ABOOMCharacter
{
	GENERATED_BODY()
	//Put somewhere else possibly
public:



	class ABOOMPatrolRoute* GetPatrolRouteComponent() const { return PatrolRouteComponent; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	class ABOOMPatrolRoute* PatrolRouteComponent;
};
