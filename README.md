# GBufferProcessPlugin
UE4 Plugin to modify Gbuffer info with material shader

UE4Version 4.27.2

插件需要配合引擎修改
Index: BasePassRendering.cpp
===================================================================
--- BasePassRendering.cpp	(revision 1633)
+++ BasePassRendering.cpp	(working copy)
@@ -953,24 +953,37 @@
 	if (ViewFamily.ViewExtensions.Num() > 0)
 	{
 		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ViewExtensionPostRenderBasePass);
-		RDG_EVENT_SCOPE(GraphBuilder, "BasePass_ViewExtensions");
-		auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
-		PassParameters->RenderTargets = BasePassRenderTargets;
-		for (auto& ViewExtension : ViewFamily.ViewExtensions)
 		{
-			for (FViewInfo& View : Views)
+			RDG_EVENT_SCOPE(GraphBuilder, "BasePass_ViewExtensions");
+			auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
+			PassParameters->RenderTargets = BasePassRenderTargets;
+			for (auto& ViewExtension : ViewFamily.ViewExtensions)
 			{
-				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
-				GraphBuilder.AddPass(
-					{},
-					PassParameters,
-					ERDGPassFlags::Raster,
-					[&ViewExtension, &View](FRHICommandListImmediate& RHICmdList)
+				for (FViewInfo& View : Views)
 				{
-					ViewExtension->PostRenderBasePass_RenderThread(RHICmdList, View);
-				});
+					RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
+					GraphBuilder.AddPass(
+						{},
+						PassParameters,
+						ERDGPassFlags::Raster,
+						[&ViewExtension, &View](FRHICommandListImmediate& RHICmdList)
+						{
+							ViewExtension->PostRenderBasePass_RenderThread(RHICmdList, View);
+						});
+				}
 			}
 		}
+
+		{
+			RDG_EVENT_SCOPE(GraphBuilder, "BasePass_ViewExtensions_Addition");
+			for (auto& ViewExtension : ViewFamily.ViewExtensions)
+			{
+				for (FViewInfo& View : Views)
+				{
+					ViewExtension->PostRenderBasePass(GraphBuilder, View);
+				}
+			}
+		}
 	}
 
 	if (bRequiresFarZQuadClear)
