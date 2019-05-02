#pragma once

namespace verus
{
	namespace CGI
	{
		enum class ClearFlags : int
		{
			color = (1 << 0),
			depth = (1 << 1),
			stencil = (1 << 2)
		};

		enum class CullMode : int
		{
			none,
			front,
			back
		};

		enum class ImageLayout : int
		{
			undefined, // Does not support device access, the contents of the memory are not guaranteed to be preserved.
			general, // Supports all types of device access.
			colorAttachmentOptimal, // Must only be used as a color or resolve attachment.
			depthStencilAttachmentOptimal, // Must only be used as a depth/stencil or depth/stencil resolve attachment.
			depthStencilReadOnlyOptimal, // Must only be used as a read-only depth/stencil attachment or as a read-only image in a shader.
			shaderReadOnlyOptimal, // Must only be used as a read-only image in a shader.
			transferSrcOptimal, // Must only be used as a source image of a transfer command.
			transferDstOptimal, // Must only be used as a destination image of a transfer command.
			presentSrc // Must only be used for presenting a presentable image for display.
		};

		enum class PolygonMode : int
		{
			fill,
			line
		};

		enum class PrimitiveTopology : int
		{
			pointList,
			lineList,
			lineStrip,
			triangleList,
			triangleStrip
		};

		struct PipelineInputAssemblyState
		{
			PrimitiveTopology _topology = PrimitiveTopology::triangleList;
			bool              _primitiveRestartEnable = false;
		};

		struct PipelineRasterizationState
		{
			PolygonMode _polygonMode = PolygonMode::fill;
			CullMode    _cullMode = CullMode::back;
			float       _depthBiasConstantFactor = 0;
			float       _depthBiasClamp = 0;
			float       _depthBiasSlopeFactor = 0;
			float       _lineWidth = 1;
			bool        _depthClampEnable = true;
			bool        _depthBiasEnable = false;
			bool        _rasterizerDiscardEnable = false;
		};
	}
}
