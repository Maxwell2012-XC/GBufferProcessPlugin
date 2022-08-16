#pragma once
#include "SceneViewExtension.h"
#include "RHI.h"
#include "RHIResources.h"

//#define MY_CHANGE_WITH_ENGINE

class UGBufferProcessSubsystem;
class UMaterialInterface;
class FRDGTexture;

class FGBufferProcessSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FGBufferProcessSceneViewExtension(const FAutoRegister& AutoRegister, UGBufferProcessSubsystem* InWorldSubsystem);
	
	//~ Begin FSceneViewExtensionBase Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {};
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override {};

#ifdef MY_CHANGE_WITH_ENGINE
	virtual void PostRenderBasePass(FRDGBuilder& GraphBuilder, FViewInfo& InView) override;
#endif
	//~ End FSceneViewExtensionBase Interface

private:
	UGBufferProcessSubsystem* WorldSubsystem;
};
