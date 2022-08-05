// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_XR_DESTROY(xr, fn) {if (XR_NULL_HANDLE != xr) {fn; xr = XR_NULL_HANDLE;}}

namespace verus
{
	namespace CGI
	{
		class BaseExtReality : public Object
		{
			struct ActionEx
			{
				XrAction        _handle = XR_NULL_HANDLE;
				Vector<XrPath>  _vSubactionPaths;
				Vector<XrSpace> _vActionSpaces;
			};
			VERUS_TYPEDEFS(ActionEx);

		protected:
			XrInstance                               _instance = XR_NULL_HANDLE;
			XrDebugUtilsMessengerEXT                 _debugUtilsMessenger = XR_NULL_HANDLE;
			XrSystemId                               _systemId = XR_NULL_SYSTEM_ID;
			XrSession                                _session = XR_NULL_HANDLE;
			XrSpace                                  _referenceSpace = XR_NULL_HANDLE;
			XrSpace                                  _headSpace = XR_NULL_HANDLE;
			XrTime                                   _predictedDisplayTime = 0;
			Vector<CSZ>                              _vRequiredExtensions;
			Vector<XrView>                           _vViews;
			Vector<XrCompositionLayerProjectionView> _vCompositionLayerProjectionViews;
			Vector<XrActionSet>                      _vActionSets;
			Vector<XrActiveActionSet>                _vActiveActionSets;
			Vector<ActionEx>                         _vActions;
			XrFormFactor                             _formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			XrViewConfigurationType                  _viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			XrEnvironmentBlendMode                   _envBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
			XrSessionState                           _sessionState = XR_SESSION_STATE_UNKNOWN;
			XrPosef                                  _identityPose;
			XrPosef                                  _headPose;
			int                                      _combinedSwapChainWidth = 0;
			int                                      _combinedSwapChainHeight = 0;
			int                                      _currentViewIndex = 0;
			int                                      _swapChainBufferCount = 0;
			int                                      _swapChainBufferIndex = 0;
			RPHandle                                 _rph;
			Vector<FBHandle>                         _fbh;
			bool                                     _sessionRunning = false;
			bool                                     _shouldRender = false;

			BaseExtReality();
			virtual ~BaseExtReality();

		public:
			virtual void Init();
			virtual void Done();

		protected:
			static XRAPI_ATTR XrBool32 XRAPI_CALL DebugUtilsMessengerCallback(
				XrDebugUtilsMessageSeverityFlagsEXT messageSeverityFlags,
				XrDebugUtilsMessageTypeFlagsEXT messageTypeFlags,
				const XrDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void* pUserData);
			bool CheckRequiredExtensions() const;
			void CreateInstance();
			void CreateDebugUtilsMessenger();
			void CreateReferenceSpace();
			void CreateHeadSpace();
			virtual XrSwapchain GetSwapChain(int viewIndex) = 0;
			virtual void GetSwapChainSize(int viewIndex, int32_t& w, int32_t& h) = 0;

		public:
			virtual void CreateActions();
			virtual void PollEvents();
			virtual void SyncActions(UINT32 activeActionSetsMask = -1);

			virtual bool GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction);
			virtual bool GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction);
			virtual bool GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction);

			virtual void BeginFrame();
			virtual int LocateViews();
			virtual void BeginView(int viewIndex, RViewDesc viewDesc);
			virtual void AcquireSwapChainImage();
			virtual void EndView(int viewIndex);
			virtual void EndFrame();

			int GetCombinedSwapChainWidth() const { return _combinedSwapChainWidth; }
			int GetCombinedSwapChainHeight() const { return _combinedSwapChainHeight; }

			RPHandle GetRenderPassHandle() const;
			FBHandle GetFramebufferHandle() const;

			const XrPosef& GetHeadPose() const { return _headPose; }
		};
		VERUS_TYPEDEFS(BaseExtReality);
	}
}
