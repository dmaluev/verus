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

			virtual void BeginRenderPass() = 0;
			virtual void EndRenderPass() = 0;

			virtual void BindVertexBuffers() = 0;
			virtual void BindIndexBuffer() = 0;

			virtual void SetScissor() = 0;
			virtual void SetViewport() = 0;

			virtual void BindPipeline() = 0;
			virtual void PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout) = 0;
			virtual void Clear(ClearFlags clearFlags) = 0;

			virtual void Draw() = 0;
			virtual void DrawIndexed() = 0;
		};
		VERUS_TYPEDEFS(BaseCommandBuffer);
	}
}
