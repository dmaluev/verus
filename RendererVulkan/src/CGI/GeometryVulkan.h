#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryVulkan : public BaseGeometry
		{
		public:
			GeometryVulkan();
			virtual ~GeometryVulkan() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;
		};
	}
}
