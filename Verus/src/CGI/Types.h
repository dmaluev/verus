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

		struct PipelineRasterizationState
		{
			PolygonMode _polygonMode = PolygonMode::fill;
			CullMode    _cullMode = CullMode::back;
			float       _depthBiasConstantFactor = 0;
			float       _depthBiasClamp = 0;
			float       _depthBiasSlopeFactor = 0;
			float       _lineWidth = 1;
			bool        _depthClampEnable = false;
			bool        _depthBiasEnable = false;
			bool        _rasterizerDiscardEnable = false;
		};

		enum class CompareOp : int
		{
			never,
			less,
			equal,
			lessOrEqual,
			greater,
			notEqual,
			greaterOrEqual,
			always,
		};

		enum class ShaderStageFlags : UINT32
		{
			vs = (1 << 0),
			hs = (1 << 1),
			ds = (1 << 2),
			gs = (1 << 3),
			fs = (1 << 4),
			cs = (1 << 5),
			vs_fs = vs | fs
		};

		enum class IeType : int
		{
			_float,
			_ubyte,
			_short
		};

		// See: https://docs.microsoft.com/en-us/windows/desktop/direct3d9/d3ddeclusage
		enum class IeUsage : int
		{
			position,
			blendWeight,
			blendIndices,
			normal,
			psize,
			texCoord,
			tangent,
			binormal,
			color
		};

		// See: https://docs.microsoft.com/en-us/windows/desktop/direct3d9/d3dvertexelement9
		struct InputElementDesc
		{
			int     _binding;
			int     _offset;
			IeType  _type;
			int     _components;
			IeUsage _usage;
			int     _usageIndex;

			static constexpr InputElementDesc End() { return { -1, -1, IeType::_float, 0, IeUsage::position, 0 }; }
		};
		VERUS_TYPEDEFS(InputElementDesc);

		enum class Sampler : int
		{
			custom,
			aniso,
			linear3D,
			nearest3D,
			count
		};
	}
}
