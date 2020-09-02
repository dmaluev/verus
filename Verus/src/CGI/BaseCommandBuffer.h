#pragma once

namespace verus
{
	namespace CGI
	{
		class BaseCommandBuffer : public Object, public Scheduled
		{
		protected:
			Vector4 _viewportSize = Vector4(0);

			BaseCommandBuffer() = default;
			virtual ~BaseCommandBuffer() = default;

		public:
			virtual void Init() = 0;
			virtual void Done() = 0;

			virtual void InitOneTimeSubmit() = 0;
			virtual void DoneOneTimeSubmit() = 0;

			virtual void Begin() = 0;
			virtual void End() = 0;

			virtual void BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle, std::initializer_list<Vector4> ilClearValues, bool setViewportAndScissor = true) = 0;
			virtual void NextSubpass() = 0;
			virtual void EndRenderPass() = 0;

			virtual void BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter = UINT32_MAX) = 0;
			virtual void BindIndexBuffer(GeometryPtr geo) = 0;

			virtual void BindPipeline(PipelinePtr pipe) = 0;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth = 0, float maxDepth = 1) = 0;
			virtual void SetScissor(std::initializer_list<Vector4> il) = 0;
			virtual void SetBlendConstants(const float* p) = 0;

			virtual bool BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle = CSHandle()) = 0;
			virtual void PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags = ShaderStageFlags::vs_fs) = 0;

			virtual void PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout,
				Range<int> mipLevels, int arrayLayer = 0) = 0;

			virtual void Draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0) = 0;
			virtual void DrawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0) = 0;

			virtual void Dispatch(int groupCountX, int groupCountY, int groupCountZ = 1) = 0;

			RcVector4 GetViewportSize() const { return _viewportSize; }
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
