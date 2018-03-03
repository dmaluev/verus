#pragma once

namespace verus
{
	namespace Scene
	{
		//! Basic camera without extra features. For shadow map computations, etc.
		class Camera
		{
		protected:
			enum class Update : BYTE
			{
				none,
				v = (1 << 0),
				p = (1 << 1)
			};

		private:
			Math::Frustum    _frustum;
			Transform3       _matV = Transform3::identity();
			Transform3       _matVi = Transform3::identity();
			Matrix4          _matP = Matrix4::identity();
			Matrix4          _matVP = Matrix4::identity();
			VERUS_PD(Point3  _posEye = Point3(0));
			VERUS_PD(Point3  _posAt = Point3(0, 0, -1));
			VERUS_PD(Vector3 _dirUp = Vector3(0, 1, 0));
			VERUS_PD(Vector3 _dirFront = Vector3(0, 0, -1));
			float            _fovY = VERUS_PI / 4; // Zero FOV means ortho.
			float            _aspectRatio = 1;
			float            _zNear = 0.1f; // 10 cm.
			float            _zFar = 10000; // 10 km.
			float            _w = 80;
			float            _h = 60;
			VERUS_PD(Update  _update = Update::none);
			VERUS_PD(void UpdateInternal());

		public:
			virtual void Update();
			void UpdateView();
			void UpdateVP();
			void UpdateFFP();

			Vector4 GetZNearFarEx() const;

			// Frustum:
			void SetFrustumNear(float zNear);
			void SetFrustumFar(float zFar);
			Math::RFrustum GetFrustum() { return _frustum; }

			// Positions:
			RcPoint3 GetPositionEye() const { return _posEye; }
			RcPoint3 GetPositionAt() const { return _posAt; }
			void MoveEyeTo(RcPoint3 pos) { _update |= Update::v; _posEye = pos; }
			void MoveAtTo(RcPoint3 pos) { _update |= Update::v; _posAt = pos; }

			// Directions:
			RcVector3 GetDirectionFront() const { return _dirFront; }
			RcVector3 GetDirectionUp() const { return _dirUp; }
			void SetDirectionUp(RcVector3 dir) { _update |= Update::v; _dirUp = dir; }

			// Matrices:
			RcTransform3 GetMatrixV() const { return _matV; }
			RcTransform3 GetMatrixVi() const { return _matVi; }
			RcMatrix4 GetMatrixP() const { return _matP; }
			RcMatrix4 GetMatrixVP() const { return _matVP; }

			// Perspective:
			float GetFOV() const { return _fovY; }
			float GetAspectRatio() const { return _aspectRatio; }
			float GetZNear() const { return _zNear; }
			float GetZFar() const { return _zFar; }
			float GetWidth() const { return _w; }
			float GetHeight() const { return _h; }
			void SetFOVH(float x);
			void SetFOV(float x) { _update |= Update::p; _fovY = x; }
			void SetAspectRatio(float x) { _update |= Update::p; _aspectRatio = x; }
			void SetZNear(float x) { _update |= Update::p; _zNear = x; }
			void SetZFar(float x) { _update |= Update::p; _zFar = x; }
			void SetWidth(float x) { _update |= Update::p; _w = (0 == x) ? _aspectRatio * _h : x; }
			void SetHeight(float x) { _update |= Update::p; _h = x; }

			void GetClippingSpacePlane(RVector4 plane) const;

			void ExcludeWaterLine(float h = 0.25f);

			// State:
			virtual void SaveState(int slot);
			virtual void LoadState(int slot);
		};
		VERUS_TYPEDEFS(Camera);

		//! The interface for getting the cursor's position.
		struct CursorPosProvider
		{
			virtual void GetPos(int& x, int& y) = 0;
		};
		VERUS_TYPEDEFS(CursorPosProvider);

		//! More advanced camera, used to show the world to the user. With motion blur.
		class MainCamera : public Camera
		{
			Matrix4            _matPrevVP = Matrix4::identity(); // For motion blur.
			PCursorPosProvider _pCpp = nullptr;
			UINT32             _currentFrame = -1;

		public:
			void operator=(const MainCamera& that);

			virtual void Update() override;
			void UpdateVP();

			void GetPickingRay(RPoint3 pos, RVector3 dir) const;

			RcMatrix4 GetMatrixPrevVP() const { return _matPrevVP; }

			void SetCursorPosProvider(PCursorPosProvider p) { _pCpp = p; }

			float ComputeMotionBlur(RcPoint3 pos, RcPoint3 posPrev) const;
		};
		VERUS_TYPEDEFS(MainCamera);
	}
}
