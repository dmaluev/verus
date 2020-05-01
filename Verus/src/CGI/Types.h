#pragma once

namespace verus
{
	namespace CGI
	{
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
			colorAttachment, // Must only be used as a color or resolve attachment.
			depthStencilAttachment, // Must only be used as a depth/stencil or depth/stencil resolve attachment.
			depthStencilReadOnly, // Must only be used as a read-only depth/stencil attachment or as a read-only image in a shader.
			xsReadOnly, // Must only be used as a read-only image in any shader.
			fsReadOnly, // Must only be used as a read-only image in fragment shader.
			transferSrc, // Must only be used as a source image of a transfer command.
			transferDst, // Must only be used as a destination image of a transfer command.
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
			triangleStrip,
			patchList3,
			patchList4
		};

		struct PipelineRasterizationState
		{
			PolygonMode _polygonMode = PolygonMode::fill;
			CullMode    _cullMode = CullMode::back;
			float       _depthBiasConstantFactor = 0;
			float       _depthBiasClamp = 0;
			float       _depthBiasSlopeFactor = 0;
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
			vs_fs = vs | fs,
			vs_hs_ds = vs | hs | ds,
			vs_hs_ds_fs = vs | hs | ds | fs
		};

		enum class IeType : int
		{
			floats,
			ubytes,
			shorts
		};

		// See: https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3ddeclusage
		enum class IeUsage : int
		{
			position,
			blendWeights,
			blendIndices,
			normal,
			tangent,
			binormal,
			color,
			psize,
			texCoord,
			instData,
			attr
		};

		// See: https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dvertexelement9
		struct VertexInputAttrDesc
		{
			int     _binding;
			int     _offset;
			IeType  _type;
			int     _components;
			IeUsage _usage;
			int     _usageIndex;

			static constexpr VertexInputAttrDesc End() { return { -1, -1, IeType::floats, 0, IeUsage::position, 0 }; }
		};
		VERUS_TYPEDEFS(VertexInputAttrDesc);

		enum class Sampler : int
		{
			custom, // Not immutable, not static sampler.
			storage, // Also known as UAV.
			input,
			shadow,
			aniso, // Most common sampler for 3D.
			linearMipL,
			nearestMipL,
			linearMipN,
			nearestMipN,
			linearClampMipL,
			nearestClampMipL,
			linearClampMipN,
			nearestClampMipN,
			count
		};

		template<typename T>
		class BaseHandle
		{
			int _h = -1;

		protected:
			static T Make(int value)
			{
				T ret;
				ret._h = value;
				return ret;
			}

		public:
			int Get() const { return _h; }
			bool IsSet() const { return _h >= 0; }
		};

		class RPHandle : public BaseHandle<RPHandle>
		{
		public:
			static RPHandle Make(int value) { return BaseHandle::Make(value); }
		};

		class FBHandle : public BaseHandle<FBHandle>
		{
		public:
			static FBHandle Make(int value) { return BaseHandle::Make(value); }
		};

		class CSHandle : public BaseHandle<CSHandle>
		{
		public:
			static CSHandle Make(int value) { return BaseHandle::Make(value); }
		};
	}
}
