// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
			VERUS_PD(Point3  _eyePos = Point3(0));
			VERUS_PD(Point3  _atPos = Point3(0, 0, -1));
			VERUS_PD(Vector3 _upDir = Vector3(0, 1, 0));
			VERUS_PD(Vector3 _frontDir = Vector3(0, 0, -1));
			float            _fovScale = 1;
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

			void EnableReflectionMode();

			Vector4 GetZNearFarEx() const;

			// Frustum:
			void SetFrustumNear(float zNear);
			void SetFrustumFar(float zFar);
			Math::RFrustum GetFrustum() { return _frustum; }

			// Positions:
			RcPoint3 GetEyePosition() const { return _eyePos; }
			RcPoint3 GetAtPosition() const { return _atPos; }
			void MoveEyeTo(RcPoint3 pos) { _update |= Update::v; _eyePos = pos; }
			void MoveAtTo(RcPoint3 pos) { _update |= Update::v; _atPos = pos; }

			// Directions:
			RcVector3 GetFrontDirection() const { return _frontDir; }
			RcVector3 GetUpDirection() const { return _upDir; }
			void SetUpDirection(RcVector3 dir) { _update |= Update::v; _upDir = dir; }

			// Matrices:
			RcTransform3 GetMatrixV() const { return _matV; }
			RcTransform3 GetMatrixVi() const { return _matVi; }
			RcMatrix4 GetMatrixP() const { return _matP; }
			RcMatrix4 GetMatrixVP() const { return _matVP; }

			// Perspective:
			float GetFovScale() const { return _fovScale; } // For converting offsets in meters to offsets in texture coords.
			float GetFovY() const { return _fovY; }
			float GetAspectRatio() const { return _aspectRatio; }
			float GetZNear() const { return _zNear; }
			float GetZFar() const { return _zFar; }
			void SetFovX(float x);
			void SetFovY(float x) { _update |= Update::p; _fovY = x; }
			void SetAspectRatio(float x) { _update |= Update::p; _aspectRatio = x; }
			void SetZNear(float x) { _update |= Update::p; _zNear = x; }
			void SetZFar(float x) { _update |= Update::p; _zFar = x; }

			// Ortho:
			float GetWidth() const { return _w; }
			float GetHeight() const { return _h; }
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
			UINT64             _currentFrame = UINT64_MAX;

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
