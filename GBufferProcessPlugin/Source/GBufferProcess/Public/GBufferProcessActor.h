#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Engine/Classes/Components/MeshComponent.h"
#include "GBufferProcessActor.generated.h"

UENUM(BlueprintType)
enum class EGBufferProcessType : uint8 
{
	SceneColor		UMETA(DisplayName = "SceneColor"),
	Normal			UMETA(DisplayName = "Normal"),
	Roughness		UMETA(DisplayName = "Roughness"),
	MAX
};

/**
 * 修改GBuffer的实例Actor
 */
UCLASS(Blueprintable)
class AGBufferProcessActor : public AActor
{
	GENERATED_UCLASS_BODY()
public:
	/** Region type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="GBuffer Modify")
	EGBufferProcessType Type;

	/** Render priority/order. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="GBuffer Modify")
	int32 Priority;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GBuffer Modify", meta = (UIMin = 0.0, UIMax = 1.0))
	float Intensity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GBuffer Modify")
	float Enabled = true;

	/** The material used to change GBuffer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GBuffer Modify")
	UMaterialInterface* Material;

#if WITH_EDITOR
	/** Called when any of the properties are changed. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** To handle play in Editor, PIE and Standalone. These methods aggregate objects in play mode similarly to 
	* Editor methods in FGBufferProcessSubsystem
	*/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 是否起作用
	virtual bool IsEffect(FVector Posi) { return true; }
};
