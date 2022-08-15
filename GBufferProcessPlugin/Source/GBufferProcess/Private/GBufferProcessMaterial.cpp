#include "GBufferProcessMaterial.h"
#include "MaterialShader.h"
#include "MaterialShaderType.h"
#include "Materials/Material.h"
#include "GlobalShader.h"
#include "TextureResource.h"
#include "ShaderCore.h"
#include "ShaderParameterMacros.h"
#include "CoreMinimal.h"
#include "CommonRenderResources.h"
#include "RenderGraph.h"
#include "RendererInterface.h"

IMPLEMENT_GLOBAL_SHADER(FGBufferProcessScreenPassVS, "/Plugin/GBufferProcessPlugin/Private/GBufferModityScreenPass.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FClearRectPS, "/Plugin/GBufferProcessPlugin/Private/GBufferModityScreenPass.usf", "ClearPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCopyTexturePS, "/Plugin/GBufferProcessPlugin/Private/GBufferModityScreenPass.usf", "CopyPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRewritePS, "/Plugin/GBufferProcessPlugin/Private/GBufferModityScreenPass.usf", "RewritePS", SF_Pixel);

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMaterialGraphRewriteNormalPS, TEXT("/Plugin/GBufferProcessPlugin/Private/GBufferModityTest.usf"), TEXT("RewriteNormalPS"), SF_Pixel);
