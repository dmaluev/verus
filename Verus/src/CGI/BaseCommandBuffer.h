#pragma once

namespace verus
{
	namespace CGI
	{
		class BaseCommandBuffer : public Object
		{
		protected:
			BaseCommandBuffer() = default;
			virtual ~BaseCommandBuffer() = default;

		public:
			virtual void Init() = 0;
			virtual void Done() = 0;

			virtual void Begin() = 0;
			virtual void End() = 0;

			virtual void BeginRenderPass(int renderPassID, int framebufferID, std::initializer_list<Vector4> ilClearValues, PcVector4 pRenderArea = nullptr) = 0;
			virtual void EndRenderPass() = 0;

			virtual void BindVertexBuffers(GeometryPtr geo) = 0;
			virtual void BindIndexBuffer(GeometryPtr geo) = 0;

			virtual void SetScissor(std::initializer_list<Vector4> il) = 0;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth = 0, float maxDepth = 1) = 0;

			virtual void BindPipeline(PipelinePtr pipe) = 0;
			virtual void PushConstant(PipelinePtr pipe, int offset, int size, const void* p) = 0;
			virtual void PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout) = 0;
			virtual void Clear(ClearFlags clearFlags) = 0;

			virtual void Draw(int vertexCount, int instanceCount, int firstVertex = 0, int firstInstance = 0) = 0;
			virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0) = 0;
		};
		VERUS_TYPEDEFS(BaseCommandBuffer);

		class CommandBufferPtr : public Ptr<BaseCommandBuffer>
		{
		public:
			void Init();
		};
		VERUS_TYPEDEFS(CommandBufferPtr);

		class CommandBufferPwn : public CommandBufferPtr
		{
		public:
			~CommandBufferPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(CommandBufferPwn);

		template<int NUM>
		class CommandBufferPwns : public Pwns<CommandBufferPwn, NUM>
		{
		};
	}
}
