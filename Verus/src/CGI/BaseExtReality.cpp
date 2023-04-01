// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

static XrResult CreateDebugUtilsMessengerEXT(XrInstance instance, const XrDebugUtilsMessengerCreateInfoEXT* pCreateInfo, XrDebugUtilsMessengerEXT* pMessenger)
{
	PFN_xrCreateDebugUtilsMessengerEXT fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, pCreateInfo, pMessenger) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

static XrResult DestroyDebugUtilsMessengerEXT(XrInstance instance, XrDebugUtilsMessengerEXT messenger)
{
	PFN_xrDestroyDebugUtilsMessengerEXT fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(messenger) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

// BaseExtReality:

BaseExtReality::BaseExtReality()
{
	VERUS_ZERO_MEM(_identityPose);
	_identityPose.orientation.w = 1;
}

BaseExtReality::~BaseExtReality()
{
	for (auto& actionEx : _vActions)
	{
		for (auto& actionSpace : actionEx._vActionSpaces)
			VERUS_XR_DESTROY(actionSpace, xrDestroySpace(actionSpace));
	}
	_vActions.clear();
	for (auto& actionSet : _vActionSets)
	{
		VERUS_XR_DESTROY(actionSet, xrDestroyActionSet(actionSet));
	}
	_vActionSets.clear();
	VERUS_XR_DESTROY(_headSpace, xrDestroySpace(_headSpace));
	VERUS_XR_DESTROY(_referenceSpace, xrDestroySpace(_referenceSpace));
	VERUS_XR_DESTROY(_session, xrDestroySession(_session));
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	VERUS_XR_DESTROY(_debugUtilsMessenger, DestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger));
#endif
	VERUS_XR_DESTROY(_instance, xrDestroyInstance(_instance));
}

void BaseExtReality::Init()
{
	UpdateAreaTransform();
}

void BaseExtReality::Done()
{
}

XRAPI_ATTR XrBool32 XRAPI_CALL BaseExtReality::DebugUtilsMessengerCallback(
	XrDebugUtilsMessageSeverityFlagsEXT messageSeverityFlags,
	XrDebugUtilsMessageTypeFlagsEXT messageTypeFlags,
	const XrDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	D::Log::Severity severity = D::Log::Severity::error;
	if (messageSeverityFlags & XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		severity = D::Log::Severity::debug;
	if (messageSeverityFlags & XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		severity = D::Log::Severity::info;
	if (messageSeverityFlags & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		severity = D::Log::Severity::warning;
	if (messageSeverityFlags & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		severity = D::Log::Severity::error;
	D::Log::I().Write(pCallbackData->message, std::this_thread::get_id(), __FILE__, __LINE__, severity);
	return XR_FALSE;
}

bool BaseExtReality::CheckRequiredExtensions() const
{
	uint32_t propertyCount = 0;
	xrEnumerateInstanceExtensionProperties(nullptr, 0, &propertyCount, nullptr);
	Vector<XrExtensionProperties> vExtensionProperties(propertyCount, { XR_TYPE_EXTENSION_PROPERTIES });
	xrEnumerateInstanceExtensionProperties(nullptr, propertyCount, &propertyCount, vExtensionProperties.data());
	Set<String> setRequiredExtensions;
	for (CSZ extensionName : _vRequiredExtensions)
		setRequiredExtensions.insert(extensionName);
	for (const auto& extensionProperties : vExtensionProperties)
		setRequiredExtensions.erase(extensionProperties.extensionName);
	return setRequiredExtensions.empty();
}

void BaseExtReality::CreateInstance()
{
	VERUS_QREF_CONST_SETTINGS;
	XrResult res = XR_SUCCESS;
	XrInstanceCreateInfo xrici = { XR_TYPE_INSTANCE_CREATE_INFO };
	strcpy_s(xrici.applicationInfo.applicationName, settings._info._appName);
	xrici.applicationInfo.applicationVersion = settings._info._appVersion;
	strcpy_s(xrici.applicationInfo.engineName, settings._info._engineName);
	xrici.applicationInfo.engineVersion = settings._info._engineVersion;
	xrici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	xrici.enabledExtensionCount = Utils::Cast32(_vRequiredExtensions.size());
	xrici.enabledExtensionNames = _vRequiredExtensions.data();
	if (XR_SUCCESS != (res = xrCreateInstance(&xrici, &_instance)))
		throw VERUS_RUNTIME_ERROR << "xrCreateInstance(); res=" << res;
}

void BaseExtReality::CreateDebugUtilsMessenger()
{
	XrResult res = XR_SUCCESS;
	XrDebugUtilsMessengerCreateInfoEXT xrdumci = { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	xrdumci.messageSeverities =
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
#endif
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	xrdumci.messageTypes =
		XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	xrdumci.userCallback = DebugUtilsMessengerCallback;
	if (XR_SUCCESS != (res = CreateDebugUtilsMessengerEXT(_instance, &xrdumci, &_debugUtilsMessenger)))
		throw VERUS_RUNTIME_ERROR << "CreateDebugUtilsMessengerEXT(); res=" << res;
}

void BaseExtReality::CreateReferenceSpace()
{
	XrResult res = XR_SUCCESS;
	XrReferenceSpaceCreateInfo xrrsci = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	xrrsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	xrrsci.poseInReferenceSpace = _identityPose;
	if (XR_SUCCESS != (res = xrCreateReferenceSpace(_session, &xrrsci, &_referenceSpace)))
		throw VERUS_RUNTIME_ERROR << "xrCreateReferenceSpace(); res=" << res;
}

void BaseExtReality::CreateHeadSpace()
{
	XrResult res = XR_SUCCESS;
	XrReferenceSpaceCreateInfo xrrsci = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	xrrsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	xrrsci.poseInReferenceSpace = _identityPose;
	if (XR_SUCCESS != (res = xrCreateReferenceSpace(_session, &xrrsci, &_headSpace)))
		throw VERUS_RUNTIME_ERROR << "xrCreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW); res=" << res;
}

void BaseExtReality::CreateActions()
{
	VERUS_QREF_IM;
	XrResult res = XR_SUCCESS;

	const int actionSetCount = im.GetActionSetCount();
	const int actionCount = im.GetActionCount();

	_vActionSets.resize(actionSetCount);
	_vActions.resize(actionCount);
	Vector<XrActionSet> vAttachActionSets;
	vAttachActionSets.reserve(actionSetCount);

	VERUS_FOR(actionSetIndex, actionSetCount)
	{
		const auto& actionSet = im.GetActionSet(actionSetIndex);
		if (!actionSet._xrCompatible)
			continue;

		XrActionSetCreateInfo xrasci = { XR_TYPE_ACTION_SET_CREATE_INFO };
		strcpy_s(xrasci.actionSetName, _C(actionSet._name));
		strcpy_s(xrasci.localizedActionSetName, _C(actionSet._localizedName));
		xrasci.priority = actionSet._priority;
		if (XR_SUCCESS != (res = xrCreateActionSet(_instance, &xrasci, &_vActionSets[actionSetIndex])))
			throw VERUS_RUNTIME_ERROR << "xrCreateActionSet(); res=" << res;

		vAttachActionSets.push_back(_vActionSets[actionSetIndex]);
	}

	VERUS_FOR(actionIndex, actionCount)
	{
		const auto& action = im.GetAction(actionIndex);

		// Create XrAction only if there are OpenXR binding paths:
		int bindingPathCount = 0;
		for (const auto& pathString : action._vBindingPaths)
		{
			if (Str::StartsWith(_C(pathString), "/user/"))
				bindingPathCount++;
		}
		if (!bindingPathCount)
			continue;

		// Filter OpenXR subaction paths:
		int subactionPathCount = 0;
		for (const auto& pathString : action._vSubactionPaths)
		{
			if (Str::StartsWith(_C(pathString), "/user/"))
				subactionPathCount++;
		}

		RActionEx actionEx = _vActions[actionIndex];
		actionEx._vSubactionPaths.reserve(subactionPathCount);
		for (const auto& pathString : action._vSubactionPaths)
		{
			if (Str::StartsWith(_C(pathString), "/user/"))
			{
				XrPath path = XR_NULL_PATH;
				if (XR_SUCCESS != (res = xrStringToPath(_instance, _C(pathString), &path)))
					throw VERUS_RUNTIME_ERROR << "xrStringToPath(); res=" << res;
				actionEx._vSubactionPaths.push_back(path);
			}
		}
		XrActionCreateInfo xraci = { XR_TYPE_ACTION_CREATE_INFO };
		strcpy_s(xraci.actionName, _C(action._name));
		strcpy_s(xraci.localizedActionName, _C(action._localizedName));
		switch (action._type)
		{
		case Input::ActionType::inBoolean:    xraci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT; break;
		case Input::ActionType::inFloat:      xraci.actionType = XR_ACTION_TYPE_FLOAT_INPUT; break;
		case Input::ActionType::inVector2:    xraci.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT; break;
		case Input::ActionType::inPose:       xraci.actionType = XR_ACTION_TYPE_POSE_INPUT; break;
		case Input::ActionType::outVibration: xraci.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT; break;
		}
		if (!actionEx._vSubactionPaths.empty())
		{
			xraci.countSubactionPaths = Utils::Cast32(actionEx._vSubactionPaths.size());
			xraci.subactionPaths = actionEx._vSubactionPaths.data();
		}
		if (XR_SUCCESS != (res = xrCreateAction(_vActionSets[action._setIndex], &xraci, &actionEx._handle)))
			throw VERUS_RUNTIME_ERROR << "xrCreateAction(); res=" << res;

		// Action Spaces:
		if (Input::ActionType::inPose == action._type)
		{
			const int actionSpaceCount = actionEx._vSubactionPaths.empty() ? 1 : Utils::Cast32(actionEx._vSubactionPaths.size());
			actionEx._vActionSpaces.resize(actionSpaceCount);
			VERUS_FOR(actionSpaceIndex, actionSpaceCount)
			{
				XrActionSpaceCreateInfo xrasci = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
				xrasci.action = actionEx._handle;
				xrasci.poseInActionSpace = _identityPose;
				if (actionSpaceIndex < actionEx._vSubactionPaths.size())
					xrasci.subactionPath = actionEx._vSubactionPaths[actionSpaceIndex];
				if (XR_SUCCESS != (res = xrCreateActionSpace(_session, &xrasci, &actionEx._vActionSpaces[actionSpaceIndex])))
					throw VERUS_RUNTIME_ERROR << "xrCreateActionSpace(); res=" << res;
			}
		}
	}

	auto SuggestInteractionProfileBindings = [this, actionCount](XrPath profilePath, CSZ supportedBindingPaths[], int supportedBindingPathCount)
	{
		VERUS_QREF_IM;
		XrResult res = XR_SUCCESS;
		Vector<XrActionSuggestedBinding> vSuggestedBindings;
		vSuggestedBindings.reserve(8);
		VERUS_FOR(actionIndex, actionCount)
		{
			if (XR_NULL_HANDLE == _vActions[actionIndex]._handle)
				continue;

			const auto& action = im.GetAction(actionIndex);
			for (const auto& pathString : action._vBindingPaths)
			{
				bool found = false;
				VERUS_FOR(pathIndex, supportedBindingPathCount)
				{
					if (pathString == supportedBindingPaths[pathIndex])
					{
						found = true;
						break;
					}
				}
				if (found)
				{
					XrPath bindingPath = XR_NULL_PATH;
					if (XR_SUCCESS != (res = xrStringToPath(_instance, _C(pathString), &bindingPath)))
						throw VERUS_RUNTIME_ERROR << "xrStringToPath(); res=" << res;
					XrActionSuggestedBinding xrasb;
					xrasb.action = _vActions[actionIndex]._handle;
					xrasb.binding = bindingPath;
					vSuggestedBindings.push_back(xrasb);
				}
			}
		}
		if (!vSuggestedBindings.empty())
		{
			XrInteractionProfileSuggestedBinding xripsb = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			xripsb.interactionProfile = profilePath;
			xripsb.suggestedBindings = vSuggestedBindings.data();
			xripsb.countSuggestedBindings = Utils::Cast32(vSuggestedBindings.size());
			if (XR_SUCCESS != (res = xrSuggestInteractionProfileBindings(_instance, &xripsb)))
				throw VERUS_RUNTIME_ERROR << "xrSuggestInteractionProfileBindings(); res=" << res;
		}
	};

	{
		XrPath profilePath = XR_NULL_PATH;
		if (XR_SUCCESS != (res = xrStringToPath(_instance, "/interaction_profiles/khr/simple_controller", &profilePath)))
			throw VERUS_RUNTIME_ERROR << "xrStringToPath(); res=" << res;
		CSZ supportedBindingPaths[] =
		{
			"/user/hand/left/input/aim/pose",
			"/user/hand/left/input/grip/pose",
			"/user/hand/left/input/menu/click",
			"/user/hand/left/input/select/click",
			"/user/hand/left/output/haptic",
			"/user/hand/right/input/aim/pose",
			"/user/hand/right/input/grip/pose",
			"/user/hand/right/input/menu/click",
			"/user/hand/right/input/select/click",
			"/user/hand/right/output/haptic"
		};
		SuggestInteractionProfileBindings(profilePath, supportedBindingPaths, VERUS_COUNT_OF(supportedBindingPaths));
	}
	{
		XrPath profilePath = XR_NULL_PATH;
		if (XR_SUCCESS != (res = xrStringToPath(_instance, "/interaction_profiles/oculus/touch_controller", &profilePath)))
			throw VERUS_RUNTIME_ERROR << "xrStringToPath(); res=" << res;
		CSZ supportedBindingPaths[] =
		{
			"/user/hand/left/input/aim/pose",
			"/user/hand/left/input/grip/pose",
			"/user/hand/left/input/menu/click",
			"/user/hand/left/input/squeeze/value",
			"/user/hand/left/input/thumbrest/touch",
			"/user/hand/left/input/thumbstick/click",
			"/user/hand/left/input/thumbstick/touch",
			"/user/hand/left/input/thumbstick/x",
			"/user/hand/left/input/thumbstick/y",
			"/user/hand/left/input/trigger/touch",
			"/user/hand/left/input/trigger/value",
			"/user/hand/left/input/x/click",
			"/user/hand/left/input/x/touch",
			"/user/hand/left/input/y/click",
			"/user/hand/left/input/y/touch",
			"/user/hand/left/output/haptic",
			"/user/hand/right/input/a/click",
			"/user/hand/right/input/a/touch",
			"/user/hand/right/input/aim/pose",
			"/user/hand/right/input/b/click",
			"/user/hand/right/input/b/touch",
			"/user/hand/right/input/grip/pose",
			"/user/hand/right/input/squeeze/value",
			"/user/hand/right/input/system/click",
			"/user/hand/right/input/thumbrest/touch",
			"/user/hand/right/input/thumbstick/click",
			"/user/hand/right/input/thumbstick/touch",
			"/user/hand/right/input/thumbstick/x",
			"/user/hand/right/input/thumbstick/y",
			"/user/hand/right/input/trigger/touch",
			"/user/hand/right/input/trigger/value",
			"/user/hand/right/output/haptic"
		};
		SuggestInteractionProfileBindings(profilePath, supportedBindingPaths, VERUS_COUNT_OF(supportedBindingPaths));
	}

	if (!vAttachActionSets.empty())
	{
		XrSessionActionSetsAttachInfo xrsasai = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
		xrsasai.countActionSets = Utils::Cast32(vAttachActionSets.size());
		xrsasai.actionSets = vAttachActionSets.data();
		if (XR_SUCCESS != (res = xrAttachSessionActionSets(_session, &xrsasai)))
			throw VERUS_RUNTIME_ERROR << "xrAttachSessionActionSets(); res=" << res;
	}

	_vActiveActionSets.reserve(8);
}

void BaseExtReality::PollEvents()
{
	XrResult res = XR_SUCCESS;
	XrEventDataBuffer eventDataBuffer = { XR_TYPE_EVENT_DATA_BUFFER };
	while (XR_SUCCESS == xrPollEvent(_instance, &eventDataBuffer))
	{
		switch (eventDataBuffer.type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			XrEventDataSessionStateChanged* pSessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventDataBuffer);
			_sessionState = pSessionStateChanged->state;
			switch (_sessionState)
			{
			case XR_SESSION_STATE_READY:
			{
				XrSessionBeginInfo xrsbi = { XR_TYPE_SESSION_BEGIN_INFO };
				xrsbi.primaryViewConfigurationType = _viewConfigurationType;
				if (XR_SUCCESS != (res = xrBeginSession(_session, &xrsbi)))
					throw VERUS_RUNTIME_ERROR << "xrBeginSession(); res=" << res;
				_runningSession = true; // A session is considered running after a successful call to xrBeginSession.
			}
			break;
			case XR_SESSION_STATE_STOPPING:
			{
				if (XR_SUCCESS != (res = xrEndSession(_session)))
					throw VERUS_RUNTIME_ERROR << "xrEndSession(); res=" << res;
				_runningSession = false; // A session becomes not running after any call is made to xrEndSession.
			}
			break;
			}
		}
		break;
		}
		eventDataBuffer = { XR_TYPE_EVENT_DATA_BUFFER };
	}
}

void BaseExtReality::SyncActions(UINT32 activeActionSetsMask)
{
	if (_sessionState != XR_SESSION_STATE_FOCUSED)
		return;

	VERUS_QREF_IM;
	XrResult res = XR_SUCCESS;

	_vActiveActionSets.clear();
	const int actionSetCount = im.GetActionSetCount();
	VERUS_FOR(actionSetIndex, actionSetCount)
	{
		if (((activeActionSetsMask >> actionSetIndex) & 0x1) && (XR_NULL_HANDLE != _vActionSets[actionSetIndex]))
		{
			XrActiveActionSet xraas = {};
			xraas.actionSet = _vActionSets[actionSetIndex];
			xraas.subactionPath = XR_NULL_PATH;
			_vActiveActionSets.push_back(xraas);
		}
	}

	if (!_vActiveActionSets.empty())
	{
		XrActionsSyncInfo xrasi = { XR_TYPE_ACTIONS_SYNC_INFO };
		xrasi.countActiveActionSets = Utils::Cast32(_vActiveActionSets.size());
		xrasi.activeActionSets = _vActiveActionSets.data();
		if (XR_SUCCESS != (res = xrSyncActions(_session, &xrasi)))
			throw VERUS_RUNTIME_ERROR << "xrSyncActions(); res=" << res;
	}
}

bool BaseExtReality::GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction)
{
	if (XR_NULL_HANDLE == _vActions[actionIndex]._handle)
		return false;

	XrActionStateGetInfo xrasgi = { XR_TYPE_ACTION_STATE_GET_INFO };
	xrasgi.action = _vActions[actionIndex]._handle;
	if (subaction >= 0)
		xrasgi.subactionPath = _vActions[actionIndex]._vSubactionPaths[subaction];

	XrActionStateBoolean xrasb = { XR_TYPE_ACTION_STATE_BOOLEAN };
	xrGetActionStateBoolean(_session, &xrasgi, &xrasb);
	currentState = !!xrasb.currentState;
	if (pChangedState)
		*pChangedState = !!xrasb.changedSinceLastSync;
	return true;
}

bool BaseExtReality::GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction)
{
	if (XR_NULL_HANDLE == _vActions[actionIndex]._handle)
		return false;

	XrActionStateGetInfo xrasgi = { XR_TYPE_ACTION_STATE_GET_INFO };
	xrasgi.action = _vActions[actionIndex]._handle;
	if (subaction >= 0)
		xrasgi.subactionPath = _vActions[actionIndex]._vSubactionPaths[subaction];

	XrActionStateFloat xrasf = { XR_TYPE_ACTION_STATE_FLOAT };
	xrGetActionStateFloat(_session, &xrasgi, &xrasf);
	currentState = xrasf.currentState;
	if (pChangedState)
		*pChangedState = !!xrasf.changedSinceLastSync;
	return true;
}

bool BaseExtReality::GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction)
{
	VERUS_RT_ASSERT(_areaUpdated);
	if (XR_NULL_HANDLE == _vActions[actionIndex]._handle)
		return false;

	XrActionStateGetInfo xrasgi = { XR_TYPE_ACTION_STATE_GET_INFO };
	xrasgi.action = _vActions[actionIndex]._handle;
	if (subaction >= 0)
		xrasgi.subactionPath = _vActions[actionIndex]._vSubactionPaths[subaction];

	XrActionStatePose xrasp = { XR_TYPE_ACTION_STATE_POSE };
	xrGetActionStatePose(_session, &xrasgi, &xrasp);
	currentState = !!xrasp.isActive;

	XrSpaceLocation xrsl = { XR_TYPE_SPACE_LOCATION };
	const XrResult res = xrLocateSpace(_vActions[actionIndex]._vActionSpaces[(subaction >= 0) ? subaction : 0],
		_referenceSpace, _predictedDisplayTime, &xrsl);
	if (XR_UNQUALIFIED_SUCCESS(res) && (xrsl.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT)))
	{
		pose = xrsl.pose;
		AreaSpacePoseToWorldSpacePose(pose);
	}
	else
		currentState = false;
	return true;
}

void BaseExtReality::BeginFrame()
{
	_shouldRender = false;
	_areaUpdated = false;

	if (!_runningSession)
		return;

	XrResult res = XR_SUCCESS;

	XrFrameState xrfs = { XR_TYPE_FRAME_STATE };
	if (XR_SUCCESS != (res = xrWaitFrame(_session, nullptr, &xrfs)))
		throw VERUS_RUNTIME_ERROR << "xrWaitFrame(); res=" << res;

	if (XR_SUCCESS != (res = xrBeginFrame(_session, nullptr)))
		throw VERUS_RUNTIME_ERROR << "xrBeginFrame(); res=" << res;

	_predictedDisplayTime = xrfs.predictedDisplayTime;
	_shouldRender = !!xrfs.shouldRender;
}

int BaseExtReality::LocateViews()
{
	if (!_shouldRender || (_sessionState != XR_SESSION_STATE_VISIBLE && _sessionState != XR_SESSION_STATE_FOCUSED))
	{
		_vCompositionLayerProjectionViews.resize(0);
		return 0;
	}

	XrResult res = XR_SUCCESS;
	XrViewLocateInfo xrvli = { XR_TYPE_VIEW_LOCATE_INFO };
	xrvli.viewConfigurationType = _viewConfigurationType;
	xrvli.displayTime = _predictedDisplayTime;
	xrvli.space = _referenceSpace;
	XrViewState xrvs = { XR_TYPE_VIEW_STATE };
	uint32_t viewCount = 0;
	if (XR_SUCCESS != (res = xrLocateViews(_session, &xrvli, &xrvs, Utils::Cast32(_vViews.size()), &viewCount, _vViews.data())))
		throw VERUS_RUNTIME_ERROR << "xrLocateViews(); res=" << res;

	if (!(xrvs.viewStateFlags & (XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT)))
	{
		_vCompositionLayerProjectionViews.resize(0);
		return 0;
	}

	_vCompositionLayerProjectionViews.resize(viewCount);
	return viewCount;
}

void BaseExtReality::BeginView(int viewIndex, RViewDesc viewDesc)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(_areaUpdated);

	_currentViewIndex = viewIndex;
	_swapChainBufferIndex = -1;

	XrCompositionLayerProjectionView& xrclpv = _vCompositionLayerProjectionViews[viewIndex];
	xrclpv = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
	xrclpv.pose = _vViews[viewIndex].pose;
	xrclpv.fov = _vViews[viewIndex].fov;
	xrclpv.subImage.swapchain = GetSwapChain(viewIndex);
	xrclpv.subImage.imageRect.offset = { 0, 0 };
	GetSwapChainSize(
		viewIndex,
		xrclpv.subImage.imageRect.extent.width,
		xrclpv.subImage.imageRect.extent.height);

	viewDesc._pose = xrclpv.pose;
	AreaSpacePoseToWorldSpacePose(viewDesc._pose);

	viewDesc._fovLeft = xrclpv.fov.angleLeft;
	viewDesc._fovRight = xrclpv.fov.angleRight;
	viewDesc._fovUp = xrclpv.fov.angleUp;
	viewDesc._fovDown = xrclpv.fov.angleDown;

	viewDesc._zNear = renderer.GetPreferredZNear();
	viewDesc._zFar = renderer.GetPreferredZFar();

	viewDesc._vpX = xrclpv.subImage.imageRect.offset.x;
	viewDesc._vpY = xrclpv.subImage.imageRect.offset.y;
	viewDesc._vpWidth = xrclpv.subImage.imageRect.extent.width;
	viewDesc._vpHeight = xrclpv.subImage.imageRect.extent.height;

	viewDesc._type = ViewType::openXR;
	viewDesc._index = viewIndex;

	viewDesc._matV = VMath::orthoInverse(Transform3(viewDesc._pose._orientation, Vector3(viewDesc._pose._position)));
	viewDesc._matP = Matrix4::MakePerspectiveOffCenter(
		viewDesc._zNear * tanf(viewDesc._fovLeft),
		viewDesc._zNear * tanf(viewDesc._fovRight),
		viewDesc._zNear * tanf(viewDesc._fovDown),
		viewDesc._zNear * tanf(viewDesc._fovUp),
		viewDesc._zNear,
		viewDesc._zFar);
}

void BaseExtReality::AcquireSwapChainImage()
{
	XrResult res = XR_SUCCESS;

	const XrSwapchain swapchain = GetSwapChain(_currentViewIndex);

	XrSwapchainImageAcquireInfo xrsiai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	uint32_t imageIndex = 0;
	if (XR_SUCCESS != (res = xrAcquireSwapchainImage(swapchain, &xrsiai, &imageIndex)))
		throw VERUS_RUNTIME_ERROR << "xrAcquireSwapchainImage(); res=" << res;

	_swapChainBufferIndex = imageIndex;

	// Wait on the oldest acquired swapchain image:
	XrSwapchainImageWaitInfo xrsiwi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	xrsiwi.timeout = XR_INFINITE_DURATION;
	if (XR_SUCCESS != (res = xrWaitSwapchainImage(swapchain, &xrsiwi)))
		throw VERUS_RUNTIME_ERROR << "xrWaitSwapchainImage(); res=" << res;
}

void BaseExtReality::EndView(int viewIndex)
{
	VERUS_RT_ASSERT(viewIndex == _currentViewIndex);
	XrResult res = XR_SUCCESS;
	XrSwapchainImageReleaseInfo xrsiri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	if (XR_SUCCESS != (res = xrReleaseSwapchainImage(GetSwapChain(viewIndex), &xrsiri)))
		throw VERUS_RUNTIME_ERROR << "xrReleaseSwapchainImage(); res=" << res;
}

void BaseExtReality::EndFrame()
{
	if (!_runningSession)
		return;

	XrResult res = XR_SUCCESS;

	XrCompositionLayerBaseHeader* pCompositionLayerBaseHeader = nullptr;
	XrCompositionLayerProjection xrclp = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	xrclp.space = _referenceSpace;
	if (!_vCompositionLayerProjectionViews.empty())
	{
		pCompositionLayerBaseHeader = reinterpret_cast<XrCompositionLayerBaseHeader*>(&xrclp);
		xrclp.viewCount = Utils::Cast32(_vCompositionLayerProjectionViews.size());
		xrclp.views = _vCompositionLayerProjectionViews.data();
	}

	// XrFrameEndInfo
	//  XrCompositionLayerProjection
	//   XrCompositionLayerProjectionView
	//    XrSwapchain

	// From each XrSwapchain only one image index is implicitly referenced per frame,
	// the one corresponding to the last call to xrReleaseSwapchainImage.
	XrFrameEndInfo xrfei = { XR_TYPE_FRAME_END_INFO };
	xrfei.displayTime = _predictedDisplayTime;
	xrfei.environmentBlendMode = _envBlendMode;
	xrfei.layerCount = pCompositionLayerBaseHeader ? 1 : 0;
	xrfei.layers = &pCompositionLayerBaseHeader;
	if (XR_SUCCESS != (res = xrEndFrame(_session, &xrfei)))
		throw VERUS_RUNTIME_ERROR << "xrEndFrame(); res=" << res;
}

RPHandle BaseExtReality::GetRenderPassHandle() const
{
	return _rph;
}

FBHandle BaseExtReality::GetFramebufferHandle() const
{
	return _fbh[_currentViewIndex * _swapChainBufferCount + _swapChainBufferIndex];
}

void BaseExtReality::BeginAreaUpdate()
{
	XrSpaceLocation xrsl = { XR_TYPE_SPACE_LOCATION };
	const XrResult res = xrLocateSpace(_headSpace, _referenceSpace, _predictedDisplayTime, &xrsl);
	if (XR_UNQUALIFIED_SUCCESS(res) && (xrsl.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT)))
	{
		_areaSpaceHeadPose = xrsl.pose;

		const Vector3 frontDir = VMath::rotate(_areaSpaceHeadPose._orientation, Vector3(0, 0, 1));
		_areaSpaceUserOffset = Vector4(Vector3(_areaSpaceHeadPose._position), atan2(frontDir.getX(), frontDir.getZ()));
		_areaSpaceUserOffset.setY(0);
	}
}

void BaseExtReality::EndAreaUpdate(PcVector4 pUserOffset)
{
	UpdateAreaTransform();

	if (pUserOffset)
	{
		_areaOrigin = _trAreaToWorld * Point3(-pUserOffset->getXYZ()) - Vector3(0, _userHeight, 0);
		_areaYaw -= pUserOffset->getW();
		UpdateAreaTransform();
	}

	_worldSpaceHeadPose = _areaSpaceHeadPose;
	AreaSpacePoseToWorldSpacePose(_worldSpaceHeadPose);

	_areaUpdated = true;
}

void BaseExtReality::SetUserHeight(float height)
{
	_userHeight = height;
}

void BaseExtReality::SetAreaYaw(float yaw)
{
	_areaYaw = yaw;
}

void BaseExtReality::SetAreaOrigin(RcPoint3 origin)
{
	_areaOrigin = origin;
}

void BaseExtReality::MoveAreaBy(RcVector3 offset)
{
	_areaOrigin += offset;
}

void BaseExtReality::TurnAreaBy(float angle)
{
	_areaYaw = Math::WrapAngle(_areaYaw + angle);
}

void BaseExtReality::TeleportUserTo(RcPoint3 pos, float yaw)
{
	Quat headYaw(_worldSpaceHeadPose._orientation);
	headYaw.setX(0);
	headYaw.setZ(0);
	const float headYawLen = VMath::length(headYaw);
	headYaw = (headYawLen >= VERUS_FLOAT_THRESHOLD) ? headYaw / headYawLen : Quat(0);

	const Vector3 userPos(
		_worldSpaceHeadPose._position.getX(),
		_areaOrigin.getY(),
		_worldSpaceHeadPose._position.getZ());

	const Transform3 trFromUserSpace = Transform3(headYaw, userPos);
	const Transform3 trToUserSpace = VMath::orthoInverse(trFromUserSpace);
	const Transform3 trNewUser = Transform3(Matrix3::rotationY(yaw), Vector3(pos));
	const Transform3 tr = trNewUser * trToUserSpace;

	_areaOrigin = tr * _areaOrigin;
	const Vector3 frontDir = tr * VMath::rotate(_areaPose._orientation, Vector3(0, 0, 1));
	_areaYaw = atan2(frontDir.getX(), frontDir.getZ());

	UpdateAreaTransform();
}

void BaseExtReality::UpdateAreaTransform()
{
	_areaPose._orientation = Quat::rotationY(_areaYaw);
	_areaPose._position = _areaOrigin + Vector3(0, _userHeight, 0);
	_trAreaToWorld = Transform3(_areaPose._orientation, Vector3(_areaPose._position));
}

void BaseExtReality::AreaSpacePoseToWorldSpacePose(Math::RPose pose)
{
	pose._orientation = _areaPose._orientation * pose._orientation;
	pose._position = _trAreaToWorld * pose._position;
}
