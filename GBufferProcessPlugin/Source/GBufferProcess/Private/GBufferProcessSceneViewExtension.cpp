// Copyright Epic Games, Inc. All Rights Reserved.
#include "GBufferProcessSceneViewExtension.h"
#include "RHI.h"
#include "SceneView.h"
#include "GBufferProcessSubsystem.h"
#include "GBufferProcessMaterial.h"
#include "CommonRenderResources.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessing.h"
#include "RenderGraph.h"
#include "PixelShaderUtils.h"
#include "DeferredShadingRenderer.h"

// Set this to 1 to clip pixels outside of bounding box.
#define CLIP_PIXELS_OUTSIDE_AABB 1

//Set this to 1 to see the clipping region.
#define GBufferProcess_SHADER_DISPLAY_BOUNDING_RECT 0

namespace
{
	FRHIDepthStencilState* GetMaterialStencilState(const FMaterial* Material)
	{
		static FRHIDepthStencilState* StencilStates[] =
		{
			TStaticDepthStencilState<false, CF_Always, true, CF_Less>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_LessEqual>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_Greater>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_GreaterEqual>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_Equal>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_NotEqual>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_Never>::GetRHI(),
			TStaticDepthStencilState<false, CF_Always, true, CF_Always>::GetRHI(),
		};
		static_assert(EMaterialStencilCompare::MSC_Count == UE_ARRAY_COUNT(StencilStates), "Ensure that all EMaterialStencilCompare values are accounted for.");

		check(Material);

		return StencilStates[Material->GetStencilCompare()];
	}

	FScreenPassTextureViewportParameters GetTextureViewportParameters(const FScreenPassTextureViewport& InViewport)
	{
		const FVector2D Extent(InViewport.Extent);
		const FVector2D ViewportMin(InViewport.Rect.Min.X, InViewport.Rect.Min.Y);
		const FVector2D ViewportMax(InViewport.Rect.Max.X, InViewport.Rect.Max.Y);
		const FVector2D ViewportSize = ViewportMax - ViewportMin;

		FScreenPassTextureViewportParameters Parameters;

		if (!InViewport.IsEmpty())
		{
			Parameters.Extent = Extent;
			Parameters.ExtentInverse = FVector2D(1.0f / Extent.X, 1.0f / Extent.Y);

			Parameters.ScreenPosToViewportScale = FVector2D(0.5f, -0.5f) * ViewportSize;
			Parameters.ScreenPosToViewportBias = (0.5f * ViewportSize) + ViewportMin;

			Parameters.ViewportMin = InViewport.Rect.Min;
			Parameters.ViewportMax = InViewport.Rect.Max;

			Parameters.ViewportSize = ViewportSize;
			Parameters.ViewportSizeInverse = FVector2D(1.0f / Parameters.ViewportSize.X, 1.0f / Parameters.ViewportSize.Y);

			Parameters.UVViewportMin = ViewportMin * Parameters.ExtentInverse;
			Parameters.UVViewportMax = ViewportMax * Parameters.ExtentInverse;

			Parameters.UVViewportSize = Parameters.UVViewportMax - Parameters.UVViewportMin;
			Parameters.UVViewportSizeInverse = FVector2D(1.0f / Parameters.UVViewportSize.X, 1.0f / Parameters.UVViewportSize.Y);

			Parameters.UVViewportBilinearMin = Parameters.UVViewportMin + 0.5f * Parameters.ExtentInverse;
			Parameters.UVViewportBilinearMax = Parameters.UVViewportMax - 0.5f * Parameters.ExtentInverse;
		}

		return Parameters;
	}

	void GetPixelSpaceBoundingRect(const FSceneView& InView, const FVector& InBoxCenter, const FVector& InBoxExtents, FIntRect& OutViewport, float& OutMaxDepth, float& OutMinDepth)
	{
		OutViewport = FIntRect(INT_MAX, INT_MAX, -INT_MAX, -INT_MAX);
		// 8 corners of the bounding box. To be multiplied by box extent and offset by the center.
		const int NumCorners = 8;
		const FVector Verts[NumCorners] = {
			FVector(1, 1, 1),
			FVector(1, 1,-1),
			FVector(1,-1, 1),
			FVector(1,-1,-1),
			FVector(-1, 1, 1),
			FVector(-1, 1,-1),
			FVector(-1,-1, 1),
			FVector(-1,-1,-1) };

		for (int32 Index = 0; Index < NumCorners; Index++)
		{
			// Project bounding box vertecies into screen space.
			const FVector WorldVert = InBoxCenter + (Verts[Index] * InBoxExtents);
			FVector2D PixelVert;
			FVector4 ScreenSpaceCoordinate = InView.WorldToScreen(WorldVert);

			OutMaxDepth = FMath::Max<float>(ScreenSpaceCoordinate.W, OutMaxDepth);
			OutMinDepth = FMath::Min<float>(ScreenSpaceCoordinate.W, OutMinDepth);

			if (InView.ScreenToPixel(ScreenSpaceCoordinate, PixelVert))
			{
				// Update screen-space bounding box with with transformed vert.
				OutViewport.Min.X = FMath::Min<int32>(OutViewport.Min.X, PixelVert.X);
				OutViewport.Min.Y = FMath::Min<int32>(OutViewport.Min.Y, PixelVert.Y);

				OutViewport.Max.X = FMath::Max<int32>(OutViewport.Max.X, PixelVert.X);
				OutViewport.Max.Y = FMath::Max<int32>(OutViewport.Max.Y, PixelVert.Y);
			}
		}
	}

	// Function that calculates all points of intersection between plane and bounding box. Resulting points are unsorted.
	void CalculatePlaneAABBIntersectionPoints(const FPlane& Plane, const FVector& BoxCenter, const FVector& BoxExtents, TArray<FVector>& OutPoints)
	{
		const FVector MaxCorner = BoxCenter + BoxExtents;

		const FVector Verts[3][4] = {
			{
				// X Direction
				FVector(-1, -1, -1),
				FVector(-1,  1, -1),
				FVector(-1, -1,  1),
				FVector(-1,  1,  1),
			},
			{
				// Y Direction
				FVector(-1, -1, -1),
				FVector( 1, -1, -1),
				FVector( 1, -1,  1),
				FVector(-1, -1,  1),
			},
			{
				// Z Direction
				FVector(-1, -1, -1),
				FVector( 1, -1, -1),
				FVector( 1,  1, -1),
				FVector(-1,  1, -1),
			}
		};

		FVector Intersection;
		FVector Start;
		FVector End;

		for (int RunningAxis_Dir = 0; RunningAxis_Dir < 3; RunningAxis_Dir++)
		{
			const FVector *CornerLocations = Verts[RunningAxis_Dir];
			for (int RunningCorner = 0; RunningCorner < 4; RunningCorner++)
			{
				Start = BoxCenter + BoxExtents * CornerLocations[RunningCorner];
				End = FVector(Start.X, Start.Y, Start.Z);
				End[RunningAxis_Dir] = MaxCorner[RunningAxis_Dir];
				if (FMath::SegmentPlaneIntersection(Start, End, Plane, Intersection))
				{
					OutPoints.Add(Intersection);
				}
			}
		}
	}

	// Takes in an existing viewport and updates it with an intersection bounding rectangle.
	void UpdateMinMaxWithFrustrumAABBIntersection(const FSceneView& InView, const FVector& InBoxCenter, const FVector& InBoxExtents, FIntRect& OutViewportToUpdate, float& OutMaxDepthToUpdate)
	{
		TArray<FVector> Points;
		Points.Reserve(6);
		CalculatePlaneAABBIntersectionPoints(InView.ViewFrustum.Planes[4], InBoxCenter, InBoxExtents, Points);
		if (Points.Num() == 0)
		{
			return;
		}

		for (FVector Point : Points)
		{
			// Project bounding box vertecies into screen space.
			FVector4 ScreenSpaceCoordinate = InView.WorldToScreen(Point);
			FVector4 ScreenSpaceCoordinateScaled = ScreenSpaceCoordinate * 1.0 / ScreenSpaceCoordinate.W;

			OutMaxDepthToUpdate = FMath::Max<float>(ScreenSpaceCoordinate.W, OutMaxDepthToUpdate);
			FVector2D PixelVert;

			if (InView.ScreenToPixel(ScreenSpaceCoordinate, PixelVert))
			{
				// Update screen-space bounding box with with transformed vert.
				OutViewportToUpdate.Min.X = FMath::Min<int32>(OutViewportToUpdate.Min.X, PixelVert.X);
				OutViewportToUpdate.Min.Y = FMath::Min<int32>(OutViewportToUpdate.Min.Y, PixelVert.Y);

				OutViewportToUpdate.Max.X = FMath::Max<int32>(OutViewportToUpdate.Max.X, PixelVert.X);
				OutViewportToUpdate.Max.Y = FMath::Max<int32>(OutViewportToUpdate.Max.Y, PixelVert.Y);
			}
		}
	}

	bool ViewSupportsRegions(const FSceneView& View)
	{
		return View.Family->EngineShowFlags.PostProcessing &&
				View.Family->EngineShowFlags.PostProcessMaterial;
	}

	template<typename TSetupFunction>
	void DrawScreenPass(
		FRHICommandListImmediate& RHICmdList,
		const FSceneView& View,
		const FScreenPassTextureViewport& OutputViewport,
		const FScreenPassTextureViewport& InputViewport,
		const FScreenPassPipelineState& PipelineState,
		TSetupFunction SetupFunction)
	{
		PipelineState.Validate();

		const FIntRect InputRect = InputViewport.Rect;
		const FIntPoint InputSize = InputViewport.Extent;
		const FIntRect OutputRect = OutputViewport.Rect;
		const FIntPoint OutputSize = OutputRect.Size();

		RHICmdList.SetViewport(OutputRect.Min.X, OutputRect.Min.Y, 0.0f, OutputRect.Max.X, OutputRect.Max.Y, 1.0f);

		SetScreenPassPipelineState(RHICmdList, PipelineState);

		// Setting up buffers.
		SetupFunction(RHICmdList);

		FIntPoint LocalOutputPos(FIntPoint::ZeroValue);
		FIntPoint LocalOutputSize(OutputSize);
		EDrawRectangleFlags DrawRectangleFlags = EDRF_UseTriangleOptimization;

		DrawPostProcessPass(
			RHICmdList,
			LocalOutputPos.X, LocalOutputPos.Y, LocalOutputSize.X, LocalOutputSize.Y,
			InputRect.Min.X, InputRect.Min.Y, InputRect.Width(), InputRect.Height(),
			OutputSize,
			InputSize,
			PipelineState.VertexShader,
			View.StereoPass,
			false,
			DrawRectangleFlags);
	}

	FVector4 Clamp(const FVector4 & VectorToClamp, float Min, float Max)
	{
		return FVector4(FMath::Clamp(VectorToClamp.X, Min, Max),
						FMath::Clamp(VectorToClamp.Y, Min, Max),
						FMath::Clamp(VectorToClamp.Z, Min, Max),
						FMath::Clamp(VectorToClamp.W, Min, Max));
	}
}

FGBufferProcessSceneViewExtension::FGBufferProcessSceneViewExtension(const FAutoRegister& AutoRegister, UGBufferProcessSubsystem* InWorldSubsystem) :
	FSceneViewExtensionBase(AutoRegister), WorldSubsystem(InWorldSubsystem)
{
}

static bool TryGetShaders(ERHIFeatureLevel::Type InFeatureLevel, FMaterialRenderProxy const*& OutMaterialProxy, FMaterial const*& OutMaterial, FMaterialShaders& OutShaders)
{
	while (OutMaterialProxy)
	{
		OutMaterial = OutMaterialProxy->GetMaterialNoFallback(InFeatureLevel);
		if (OutMaterial && OutMaterial->IsLightFunction())
		{
			FMaterialShaderTypes ShaderTypes;
			ShaderTypes.AddShaderType<FMaterialGraphRewriteNormalPS>();
			if (OutMaterial->TryGetShaders(ShaderTypes, nullptr, OutShaders))
			{
				return true;
			}
		}
		OutMaterialProxy = OutMaterialProxy->GetFallback(InFeatureLevel);
	}
	return false;
}


#ifdef MY_CHANGE_WITH_ENGINE
void FGBufferProcessSceneViewExtension::PostRenderBasePass(FRDGBuilder& GraphBuilder, FViewInfo& InView)
{
	if (!WorldSubsystem){
		return;
	}

	AGBufferProcessActor* ModifyActor = WorldSubsystem->GetEffectModifyActor();
	if (!ModifyActor){
		return;
	}

	UMaterialInterface* ModifyMaterial = ModifyActor->Material;
	if (!ModifyMaterial) {
		return;
	}

	FMaterialRenderProxy* ModifyMaterialProxy = ModifyMaterial->GetRenderProxy();
	if (!ModifyMaterialProxy) {
		return;
	}

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);
	FIntPoint RT_Size = SceneContext.GetBufferSizeXY();

	TStaticArray<FRDGTextureRef, MaxSimultaneousRenderTargets> BasePassTextures;
	int32 GBufferDIndex = INDEX_NONE;
	uint32 BasePassTextureCount = SceneContext.GetGBufferRenderTargets(GraphBuilder, BasePassTextures, GBufferDIndex);
	TArrayView<FRDGTextureRef> BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);

	// 创建默认的混合状态
	FRHIBlendState* DefaultBlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();

	// 绘制的默认ViewportSize
	const FScreenPassTextureViewport RegionViewport(SceneContext.GetBufferSizeXY());

	// Step 1 : copy normal到临时buffer
#pragma region COPY
		// 创建临时的RDG纹理描述
	FRDGTextureDesc NormalTextureDesc = BasePassTexturesView[1]->Desc;
	NormalTextureDesc.Reset();
	NormalTextureDesc.ClearValue = FClearValueBinding::Black;
	NormalTextureDesc.Flags |= (TexCreate_RenderTargetable | TexCreate_ShaderResource);
	// 创建临时的RDG纹理.
	FRDGTextureRef BackRDGNormalTexture = GraphBuilder.CreateTexture(NormalTextureDesc, TEXT("BackRDGNormalTexture"));

	//创建Copy PS Shader的参数
	FCopyTexturePS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyTexturePS::FParameters>();

	TShaderMapRef<FGBufferProcessScreenPassVS> ScreenPassVS(GlobalShaderMap);
	TShaderMapRef<FCopyTexturePS> CopyPixelShader(GlobalShaderMap);

	// 设置Normal为SRV，同时设置Point Sample
	Parameters->SrcTexture = BasePassTexturesView[1];
	Parameters->SrcTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();

	// 设置RTV
	Parameters->RenderTargets[0] = FRenderTargetBinding(BackRDGNormalTexture, ERenderTargetLoadAction::EClear);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("CopyNormal"),
		Parameters,
		ERDGPassFlags::Raster,
		[&InView, ScreenPassVS, CopyPixelShader, RegionViewport, Parameters, DefaultBlendState](FRHICommandListImmediate& RHICmdList)
		{
			DrawScreenPass(
				RHICmdList,
				InView,
				RegionViewport,
				RegionViewport,
				FScreenPassPipelineState(ScreenPassVS, CopyPixelShader, DefaultBlendState),
				[&](FRHICommandListImmediate&)
				{
					SetShaderParameters(RHICmdList, CopyPixelShader, CopyPixelShader.GetPixelShader(), *Parameters);
				});
		});
#pragma endregion

	// Step 2 : 临时Normal Buffer写回 GBuffer Normal上
#pragma region REWRITE
	//创建Rewrite PS Shader的参数
	const auto FeatureLevel = InView.GetFeatureLevel();
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	const FMaterial* MaterialResource = &ModifyMaterialProxy->GetMaterialWithFallback(FeatureLevel, MaterialRenderProxy);
	MaterialRenderProxy = MaterialRenderProxy ? MaterialRenderProxy : ModifyMaterialProxy;

	typename FMaterialGraphRewriteNormalPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<typename FMaterialGraphRewriteNormalPS::FPermutationTest>(true);

	FMaterialGraphRewriteNormalPS::FParameters* RewriteParameters =
		GraphBuilder.AllocParameters<FMaterialGraphRewriteNormalPS::FParameters>();

	FMaterialShaders MaterialShaders;
	const FMaterial* MaterialForRendering = nullptr;
	if (!TryGetShaders(FeatureLevel, MaterialRenderProxy, MaterialForRendering, MaterialShaders))
	{
		return;
	}

	TShaderRef<FMaterialGraphRewriteNormalPS> RewritePsShader;
	MaterialShaders.TryGetPixelShader(RewritePsShader);
	if (!RewritePsShader.IsValid())
	{
		return;
	}

	// 设置RTV为 Gbuffer normal
	RewriteParameters->RenderTargets[0] = FRenderTargetBinding(BasePassTexturesView[1], ERenderTargetLoadAction::ELoad);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("RewriteNormal"),
		RewriteParameters,
		ERDGPassFlags::Raster,
		[&InView, ScreenPassVS, RewritePsShader, MaterialResource, MaterialRenderProxy, RegionViewport, RewriteParameters, DefaultBlendState](FRHICommandListImmediate& RHICmdList)
		{
			DrawScreenPass(
				RHICmdList,
				InView,
				RegionViewport,
				RegionViewport,
				FScreenPassPipelineState(ScreenPassVS, RewritePsShader, DefaultBlendState),
				[&InView, ScreenPassVS, RewritePsShader, MaterialResource, MaterialRenderProxy](FRHICommandListImmediate& RHICmdList)
				{
					RewritePsShader->SetParameters(RHICmdList, InView, MaterialRenderProxy, *MaterialResource);
				});
		});
#pragma endregion

#if 0
	// 取得GbufferData
	const auto FeatureLevel = InView.GetFeatureLevel();
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);
	FIntPoint RT_Size = SceneContext.GetBufferSizeXY();

	TStaticArray<FRDGTextureRef, MaxSimultaneousRenderTargets> BasePassTextures;
	int32 GBufferDIndex = INDEX_NONE;
	uint32 BasePassTextureCount = SceneContext.GetGBufferRenderTargets(GraphBuilder, BasePassTextures, GBufferDIndex);
	TArrayView<FRDGTextureRef> BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);

	// 创建默认的混合状态
	FRHIBlendState* DefaultBlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();

	// 绘制的默认ViewportSize
	const FScreenPassTextureViewport RegionViewport(SceneContext.GetBufferSizeXY());

	// Step 1 : copy normal到临时buffer
#pragma region COPY
		// 创建临时的RDG纹理描述
		FRDGTextureDesc NormalTextureDesc = BasePassTexturesView[1]->Desc;
		NormalTextureDesc.Reset();
		NormalTextureDesc.ClearValue = FClearValueBinding::Black;
		NormalTextureDesc.Flags |= (TexCreate_RenderTargetable| TexCreate_ShaderResource);
		// 创建临时的RDG纹理.
		FRDGTextureRef BackRDGNormalTexture = GraphBuilder.CreateTexture(NormalTextureDesc, TEXT("BackRDGNormalTexture"));

		//创建Copy PS Shader的参数
		FCopyTexturePS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyTexturePS::FParameters>();

		TShaderMapRef<FGBufferModityScreenPassVS> ScreenPassVS(GlobalShaderMap);
		TShaderMapRef<FCopyTexturePS> CopyPixelShader(GlobalShaderMap);

		// 设置Normal为SRV，同时设置Point Sample
		Parameters->SrcTexture = BasePassTexturesView[1];
		Parameters->SrcTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();

		// 设置RTV
		Parameters->RenderTargets[0] = FRenderTargetBinding(BackRDGNormalTexture, ERenderTargetLoadAction::EClear);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("CopyNormal"),
			Parameters,
			ERDGPassFlags::Raster,
			[&InView, ScreenPassVS, CopyPixelShader, RegionViewport, Parameters, DefaultBlendState](FRHICommandListImmediate& RHICmdList)
			{
				DrawScreenPass(
					RHICmdList,
					InView,
					RegionViewport,
					RegionViewport,
					FScreenPassPipelineState(ScreenPassVS, CopyPixelShader, DefaultBlendState),
					[&](FRHICommandListImmediate&)
					{
						SetShaderParameters(RHICmdList, CopyPixelShader, CopyPixelShader.GetPixelShader(), *Parameters);
					});
			});
#pragma endregion

	// Step 2 : 临时Normal Buffer写回 GBuffer Normal上
#pragma region REWRITE
		//创建Rewrite PS Shader的参数
		FRewritePS::FParameters* RewriteParameters = GraphBuilder.AllocParameters<FRewritePS::FParameters>();

		TShaderMapRef<FRewritePS> RewritePixelShader(GlobalShaderMap);

		// 设置Normal为SRV，同时设置Point Sample
		RewriteParameters->SrcTexture = BackRDGNormalTexture;
		RewriteParameters->SrcTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();

		// 设置RTV为 Gbuffer normal
		RewriteParameters->RenderTargets[0] = FRenderTargetBinding(BasePassTexturesView[1], ERenderTargetLoadAction::ELoad);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("RewriteNormal"),
			RewriteParameters,
			ERDGPassFlags::Raster,
			[&InView, ScreenPassVS, RewritePixelShader, RegionViewport, RewriteParameters, DefaultBlendState](FRHICommandListImmediate& RHICmdList)
			{
				DrawScreenPass(
					RHICmdList,
					InView,
					RegionViewport,
					RegionViewport,
					FScreenPassPipelineState(ScreenPassVS, RewritePixelShader, DefaultBlendState),
					[&](FRHICommandListImmediate&)
					{
						SetShaderParameters(RHICmdList, RewritePixelShader, RewritePixelShader.GetPixelShader(), *RewriteParameters);
					});
			});
#pragma endregion
#endif
}
#endif
