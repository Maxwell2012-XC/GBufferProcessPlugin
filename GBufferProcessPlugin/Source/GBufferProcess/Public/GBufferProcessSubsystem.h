#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Subsystems/EngineSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "GBufferProcessActor.h"
#include "GBufferProcessSceneViewExtension.h"

#if WITH_EDITOR
#include "EditorUndoClient.h"
#endif

#include "GBufferProcessSubsystem.generated.h"

/**
 * Conditional inheritance to allow UGBufferProcessSubsystem to inherit/avoid Editor's Undo/Redo in Editor/Game modes.
 */ 
#if WITH_EDITOR
class FGBufferProcessEditorUndoClient : public FEditorUndoClient
{
};
#else
class FGBufferProcessEditorUndoClient
{
};
#endif

/**
 * UGBufferProcessSubsystem目的是通过插件的方式，把修改GBuffer信息的操作暴露给材质蓝图
 */
UCLASS()
class UGBufferProcessSubsystem : public UWorldSubsystem, public FGBufferProcessEditorUndoClient
{
	GENERATED_BODY()
public:

	// Subsystem Init/Deinit
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Undo/redo is only supported by editor.
#if WITH_EDITOR
	// FEditorUndoClient pure virtual methods.
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
#endif

public:
	UFUNCTION()
	void OnActorSpawned(AActor* InActor);

	UFUNCTION()
	void OnActorDeleted(AActor* InActor);

#if WITH_EDITOR
	/** A callback for when the level is loaded. */
	UFUNCTION()
	void OnLevelActorListChanged() { PostUndo(true); };
#endif

	/** Sorts regions based on priority. */
	void SortRegionsByPriority();

	//
	AGBufferProcessActor* GetEffectModifyActor();

public:
	/** Stores pointers to all GBufferProcessActor Actors. */
	TArray<AGBufferProcessActor*> Regions;

private:
	/** Region class. Used for getting all region actors in level. */
	TSubclassOf<AGBufferProcessActor> RegionClass;
	
	TSharedPtr< class FGBufferProcessSceneViewExtension, ESPMode::ThreadSafe > PostProcessSceneViewExtension;

	FCriticalSection RegionAccessCriticalSection;

public:
	friend class FGBufferProcessSceneViewExtension;
};
