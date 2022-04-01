// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		enum class CompareOp : int
		{
			never,
			less,           // <
			equal,          // ==
			lessOrEqual,    // <=
			greater,        // >
			notEqual,       // !=
			greaterOrEqual, // >=
			always
		};

		enum class CubeMapFace : int
		{
			posX,
			negX,
			posY,
			negY,
			posZ,
			negZ,
			count,
			all, // Match texture index to cube map face when creating framebuffer.
			none = -1
		};

		enum class PolygonMode : int
		{
			fill,
			line
		};

		enum class CullMode : int
		{
			none,
			front,
			back
		};

		struct PipelineRasterizationState
		{
			PolygonMode _polygonMode = PolygonMode::fill;
			CullMode    _cullMode = CullMode::back;
			float       _depthBiasConstantFactor = 0;
			float       _depthBiasClamp = 0;
			float       _depthBiasSlopeFactor = 0;
			bool        _depthBiasEnable = false;
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

		enum class Sampler : int
		{
			custom, // Not immutable, not static sampler.
			input,
			storage, // Also known as UAV.
			lodBias,
			shadow,
			aniso, // Most common sampler for 3D.
			anisoClamp,
			anisoSharp, // For UI.
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

		enum class ShaderStageFlags : UINT32
		{
			// Classic pipeline:
			vs = (1 << 0), // Vertex shader.
			hs = (1 << 1), // Hull shader. Also known as tessellation control shader.
			ds = (1 << 2), // Domain shader. Also known as tessellation evaluation shader.
			gs = (1 << 3), // Geometry shader.
			fs = (1 << 4), // Fragment shader. Also known as pixel shader.

			cs = (1 << 5), // Compute shader. Also known as kernel shader.

			// Raytracing:
			rtrg = (1 << 6), // Ray generation shader.
			rtah = (1 << 7), // Any hit shader.
			rtch = (1 << 8), // Closest hit shader.
			rtm = (1 << 9), // Miss shader.
			rti = (1 << 10), // Intersection shader.
			rtc = (1 << 11), // Callable shader.

			// Mesh shading pipeline:
			ts = (1 << 12), // Task shader. Also known as amplification shader.
			ms = (1 << 13), // Mesh shader.

			vs_fs = vs | fs,
			vs_hs_ds = vs | hs | ds,
			vs_hs_ds_fs = vs | hs | ds | fs
		};

		enum class ViaType : int
		{
			floats,
			halfs,
			shorts,
			ubytes
		};

		// See: https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3ddeclusage
		enum class ViaUsage : int
		{
			position, // "POSITION" in glTF.
			blendWeights, // "WEIGHTS_0" in glTF.
			blendIndices, // "JOINTS_0" in glTF.
			normal, // "NORMAL" in glTF.
			tangent, // "TANGENT" in glTF.
			binormal,
			color, // "COLOR_X" in glTF.
			psize,
			texCoord, // "TEXCOORD_X" in glTF.
			instData,
			attr
		};

		// See: https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dvertexelement9
		struct VertexInputAttrDesc
		{
			int      _binding;
			int      _offset;
			ViaType  _type;
			int      _components;
			ViaUsage _usage;
			int      _usageIndex;

			static constexpr VertexInputAttrDesc End() { return { -1, -1, ViaType::floats, 0, ViaUsage::position, 0 }; }
		};
		VERUS_TYPEDEFS(VertexInputAttrDesc);

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
