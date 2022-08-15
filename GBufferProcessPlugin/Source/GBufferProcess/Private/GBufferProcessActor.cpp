#include "GBufferProcessActor.h"
#include "GBufferProcessSubsystem.h"
#include "Engine/Classes/Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"

AGBufferProcessActor::AGBufferProcessActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
	, Type(EGBufferProcessType::Normal)
	, Priority(0)
	, Intensity(1.0)
{
}

void AGBufferProcessActor::BeginPlay()
{	
	UGBufferProcessSubsystem* GBufferProcessSubsystem = static_cast<UGBufferProcessSubsystem*>(this->GetWorld()->GetSubsystemBase(UGBufferProcessSubsystem::StaticClass()));
	if (GBufferProcessSubsystem)
	{
		GBufferProcessSubsystem->OnActorSpawned(this);
	}
}

void AGBufferProcessActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGBufferProcessSubsystem* GBufferProcessSubsystem = static_cast<UGBufferProcessSubsystem*>(this->GetWorld()->GetSubsystemBase(UGBufferProcessSubsystem::StaticClass()));
	if (GBufferProcessSubsystem)
	{
		GBufferProcessSubsystem->OnActorDeleted(this);
	}
}

#if WITH_EDITOR
void AGBufferProcessActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGBufferProcessActor, Priority))
	{
		UGBufferProcessSubsystem* GBufferProcessSubsystem = static_cast<UGBufferProcessSubsystem*>(this->GetWorld()->GetSubsystemBase(UGBufferProcessSubsystem::StaticClass()));
		if (GBufferProcessSubsystem)
		{
			GBufferProcessSubsystem->SortRegionsByPriority();
		}
	}
}
#endif //WITH_EDITOR

