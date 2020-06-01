#pragma once

namespace verus
{
	namespace Scene
	{
		enum class DrawDepth : int
		{
			no,
			yes,
			automatic
		};

		class SceneManager : public Singleton<SceneManager>, public Object
		{
			Math::Octree _octree;
			PCamera      _pCamera = nullptr;
			PMainCamera  _pMainCamera = nullptr;

		public:
			SceneManager();
			virtual ~SceneManager();

			void Init();
			void Done();

			void UpdateParts();

			// Camera:
			PCamera GetCamera() const { return _pCamera; }
			PCamera SetCamera(PCamera p) { return Utils::Swap(_pCamera, p); }
			PMainCamera GetMainCamera() const { return _pMainCamera; }
			PMainCamera SetCamera(PMainCamera p) { _pCamera = p; return Utils::Swap(_pMainCamera, p); }

			static bool IsDrawingDepth(DrawDepth dd);

			bool RayCastingTest(RcPoint3 pointA, RcPoint3 pointB, void* pBlock = nullptr,
				PPoint3 pPoint = nullptr, PVector3 pNormal = nullptr, const float* pRadius = nullptr,
				Physics::Group mask = Physics::Group::immovable | Physics::Group::terrain);
		};
		VERUS_TYPEDEFS(SceneManager);
	}
}
