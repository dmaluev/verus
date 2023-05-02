// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		// View can be a smaller portion of framebuffer with an offset.
		enum class ViewportScissorFlags : UINT32
		{
			none = 0,
			setViewportForFramebuffer = (1 << 0),
			setViewportForCurrentView = (1 << 1),
			setScissorForFramebuffer = (1 << 2),
			setScissorForCurrentView = (1 << 3),
			applyOffscreenScale = (1 << 4),
			applyHalfScale = (1 << 5),
			setAllForFramebuffer = setViewportForFramebuffer | setScissorForFramebuffer,
			setAllForCurrentView = setViewportForCurrentView | setScissorForCurrentView,
			setAllForCurrentViewScaled = setAllForCurrentView | applyOffscreenScale
		};

		class BaseCommandBuffer : public Object, public Scheduled
		{
		protected:
			Vector4 _viewportSize = Vector4(0);
			Vector4 _viewScaleBias = Vector4(0);

			void SetViewportAndScissor(ViewportScissorFlags vsf, int width, int height);

			BaseCommandBuffer() = default;
			virtual ~BaseCommandBuffer() = default;

		public:
			virtual void Init() = 0;
			virtual void Done() = 0;

			virtual void InitOneTimeSubmit() = 0;
			virtual void DoneOneTimeSubmit() = 0;

			virtual void Begin() = 0;
			virtual void End() = 0;

			virtual void PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout,
				Range mipLevels, Range arrayLayers = 0) = 0;

			// <RenderPass>
			virtual void BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle,
				std::initializer_list<Vector4> ilClearValues, ViewportScissorFlags vsf = ViewportScissorFlags::setAllForCurrentViewScaled) = 0;
			virtual void NextSubpass() = 0;
			virtual void EndRenderPass() = 0;
			// </RenderPass>

			// <Pipeline>
			virtual void BindPipeline(PipelinePtr pipe) = 0;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth = 0, float maxDepth = 1) = 0;
			virtual void SetScissor(std::initializer_list<Vector4> il) = 0;
			virtual void SetBlendConstants(const float* p) = 0;
			// </Pipeline>

			// <VertexInput>
			virtual void BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter = UINT32_MAX) = 0;
			virtual void BindIndexBuffer(GeometryPtr geo) = 0;
			// </VertexInput>

			// <Descriptors>
			virtual bool BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle = CSHandle()) = 0;
			virtual bool BindDescriptors(ShaderPtr shader, int setNumber, GeometryPtr geo, int sbIndex) = 0;
			virtual void PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags = ShaderStageFlags::vs_fs) = 0;
			// </Descriptors>

			// <Draw>
			virtual void Draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0) = 0;
			virtual void DrawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0) = 0;
			virtual void Dispatch(int groupCountX, int groupCountY, int groupCountZ = 1) = 0;
			virtual void DispatchIndirect() {} // WIP.
			virtual void DispatchMesh(int groupCountX, int groupCountY, int groupCountZ) {} // WIP.
			virtual void TraceRays(int width, int height, int depth) {} // WIP.
			// </Draw>

			// <Profiler>
			virtual void ProfilerBeginEvent(UINT32 color, CSZ text) {}
			virtual void ProfilerEndEvent() {}
			virtual void ProfilerSetMarker(UINT32 color, CSZ text) {}
			// </Profiler>

			RcVector4 GetViewportSize() const { return _viewportSize; }
			RcVector4 GetViewScaleBias() const { return _viewScaleBias; }
		};
		VERUS_TYPEDEFS(BaseCommandBuffer);

		class CommandBufferPtr : public Ptr<BaseCommandBuffer>
		{
		public:
			void Init();
			void InitOneTimeSubmit();
		};
		VERUS_TYPEDEFS(CommandBufferPtr);

		class CommandBufferPwn : public CommandBufferPtr
		{
		public:
			~CommandBufferPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(CommandBufferPwn);

		template<int COUNT>
		class CommandBufferPwns : public Pwns<CommandBufferPwn, COUNT>
		{
		};
	}
}
