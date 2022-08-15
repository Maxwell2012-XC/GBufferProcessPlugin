#include "GBufferProcessSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "SceneViewExtension.h"
#include "GBufferProcessSceneViewExtension.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	bool IsRegionValid(AGBufferProcessActor* InRegion, UWorld* CurrentWorld)
	{
		// There some cases in which actor can belong to a different world or the world without subsystem.
		// Example: when editing a blueprint deriving from AVPCRegion.
		// We also check if the actor is being dragged from the content browser.
#if WITH_EDITOR
		return InRegion && !InRegion->bIsEditorPreviewActor && InRegion->GetWorld() == CurrentWorld;
#else
		return InRegion && InRegion->GetWorld() == CurrentWorld;
#endif
	}
}

void UGBufferProcessSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UGBufferProcessSubsystem::OnActorSpawned);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UGBufferProcessSubsystem::OnActorDeleted);
		GEngine->OnLevelActorListChanged().AddUObject(this, &UGBufferProcessSubsystem::OnLevelActorListChanged);
		GEditor->RegisterForUndo(this);
	}
#endif
	// Initializing Scene view extension responsible for rendering regions.
	PostProcessSceneViewExtension = FSceneViewExtensions::NewExtension<FGBufferProcessSceneViewExtension>(this);
}

void UGBufferProcessSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
		GEditor->UnregisterForUndo(this);
	}
#endif
	Regions.Reset();
	PostProcessSceneViewExtension.Reset();
	PostProcessSceneViewExtension = nullptr;
}

void UGBufferProcessSubsystem::OnActorSpawned(AActor* InActor)
{
	AGBufferProcessActor* AsRegion = Cast<AGBufferProcessActor>(InActor);

	if (IsRegionValid(AsRegion, GetWorld()))
	{
		FScopeLock RegionScopeLock(&RegionAccessCriticalSection);
		Regions.Add(AsRegion);
		SortRegionsByPriority();
	}
}

void UGBufferProcessSubsystem::OnActorDeleted(AActor* InActor)
{
	AGBufferProcessActor* AsRegion = Cast<AGBufferProcessActor>(InActor);
	if (IsRegionValid(AsRegion, GetWorld()))
	{
		FScopeLock RegionScopeLock(&RegionAccessCriticalSection);
		Regions.Remove(AsRegion);
	}
}

#if WITH_EDITOR
void UGBufferProcessSubsystem::PostUndo(bool bSuccess)
{
	FScopeLock RegionScopeLock(&RegionAccessCriticalSection);

	Regions.Reset();
	for (TActorIterator<AGBufferProcessActor> It(GetWorld()); It; ++It)
	{
		AGBufferProcessActor* AsRegion = *It;
		if (IsRegionValid(AsRegion, GetWorld()))
		{
			Regions.Add(AsRegion);
		}
	}
	SortRegionsByPriority();
}
#endif

void UGBufferProcessSubsystem::SortRegionsByPriority()
{
	FScopeLock RegionScopeLock(&RegionAccessCriticalSection);

	Regions.Sort([](const AGBufferProcessActor& A, const AGBufferProcessActor& B) {
		// Regions with the same priority could potentially cause flickering on overlap
		return A.Priority < B.Priority;
	});
}

AGBufferProcessActor* UGBufferProcessSubsystem::GetEffectModifyActor()
{
	for (auto it : Regions)
	{
		if (it->IsEffect(FVector::ZeroVector))
		{
			return it;
		}
	}
	return nullptr;
}

