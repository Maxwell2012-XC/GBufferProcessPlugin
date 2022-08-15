#pragma once
#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "RHI.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Runtime/RHI/Public/RHIStaticStates.h"
#include "Runtime/RenderCore/Public/ShaderParameterUtils.h"
#include "Runtime/RenderCore/Public/RenderResource.h"
#include "Runtime/Renderer/Public/MaterialShader.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"
#include "Runtime/Renderer/Private/ScreenPass.h"
#include "Runtime/Renderer/Private/SceneTextureParameters.h"
#include "GBufferProcessActor.h"

// The vertex shader used by DrawScreenPass to draw a rectangle.
class FGBufferProcessScreenPassVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGBufferProcessScreenPassVS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters&)
	{
		return true;
	}

	FGBufferProcessScreenPassVS() = default;
	FGBufferProcessScreenPassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

// A simple shader that outputs (0.,0.,0.,0.)
class FClearRectPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearRectPS);
	SHADER_USE_PARAMETER_STRUCT(FClearRectPS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		//FGBufferModityMaterialPS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		//OutEnvironment.SetDefine(TEXT("CLEAR_PS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

// A simple shader that copy texture
class FCopyTexturePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCopyTexturePS);
	SHADER_USE_PARAMETER_STRUCT(FCopyTexturePS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SrcTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SrcTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

// A simple shader that copy texture
class FRewritePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRewritePS);
	SHADER_USE_PARAMETER_STRUCT(FRewritePS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SrcTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SrcTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

// a test shader driver from material graph
class FMaterialGraphRewriteNormalPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FMaterialGraphRewriteNormalPS, Material);

public:
	class FPermutationTest : SHADER_PERMUTATION_BOOL("PermutationTest");
	using FPermutationDomain = TShaderPermutationDomain<
		FPermutationTest>;

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FMaterialGraphRewriteNormalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMaterialShader(Initializer)
	{
	}

	FMaterialGraphRewriteNormalPS() {}

public:
	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FRHIPixelShader* ShaderRHI = RHICmdList.GetBoundPixelShader();

		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

public:
	//LAYOUT_FIELD(FShaderUniformBufferParameter, PassUniformBuffer);
};
