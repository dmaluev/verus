// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

// Cutscene::Command:

void Cutscene::Command::Parse(pugi::xml_node node)
{
	_url = node.attribute("url").value();

	CSZ delay = node.attribute("delay").value();
	if ('^' == *delay)
	{
		_prevDelay = static_cast<float>(atof(delay + 1));
		_delay = FLT_MAX;
	}
	else if (']' == *delay)
	{
		_endDelay = static_cast<float>(atof(delay + 1));
		_delay = FLT_MAX;
	}
	else
	{
		_delay = static_cast<float>(atof(delay));
	}

	_duration = node.attribute("duration").as_float(_duration);
}

bool Cutscene::Command::Update()
{
	const float time = GetTime();
	if (time >= _duration)
	{
		OnEnd();
		return false;
	}
	return true;
}

void Cutscene::Command::UpdatePrevDelay(float newActiveDuration)
{
	if (_prevDelay != FLT_MAX)
	{
		const float delay = newActiveDuration + _prevDelay;
		_delay = (FLT_MAX == _delay) ? delay : Math::Max(_delay, delay);
	}
}

void Cutscene::Command::UpdateEndDelay(float newActiveDuration)
{
	if (_endDelay != FLT_MAX)
	{
		const float delay = newActiveDuration + _endDelay;
		_delay = (FLT_MAX == _delay) ? delay : Math::Max(_delay, delay);
	}
}

void Cutscene::Command::LimitActiveDuration(float limit)
{
	VERUS_RT_ASSERT(_delay != FLT_MAX && _duration != FLT_MAX);
	const float activeDuration = _delay + _duration;
	const float diff = activeDuration - limit;
	if (diff > 0)
	{
		const float newDuration = _duration - diff;
		_duration = Math::Max(0.f, newDuration);
		if (newDuration < 0)
			_delay += newDuration;
	}
}

// Cutscene::BarrierCommand:

Cutscene::PCommand Cutscene::BarrierCommand::Make()
{
	return new BarrierCommand;
}

// Cutscene::CameraCommand:

Cutscene::CameraCommand::~CameraCommand()
{
	IO::Async::Cancel(this);
}

Cutscene::PCommand Cutscene::CameraCommand::Make()
{
	return new CameraCommand;
}

void Cutscene::CameraCommand::Parse(pugi::xml_node node)
{
	Command::Parse(node);

	_duration = FLT_MAX;
	IO::Async::I().Load(_C(_url), this);
}

bool Cutscene::CameraCommand::Update()
{
	if (IsDelayed() || FLT_MAX == _duration)
		return true;

	const float time = GetTime();
	if (time >= _duration)
	{
		OnEnd();
		return false;
	}

	_motion.ProcessTriggers(time, this);

	_pCutscene->_camera.ApplyMotion(_C(_cameraName), _motion, time);

	if (!_pCutscene->_pCurrentCameraCommand)
		_pCutscene->_pCurrentCameraCommand = this;

	return true;
}

void Cutscene::CameraCommand::OnBegin()
{
	Command::OnBegin();

	if (!_pCutscene->_pCurrentCameraCommand)
		_pCutscene->_pCurrentCameraCommand = this;
}

void Cutscene::CameraCommand::OnEnd()
{
	Command::OnEnd();

	if (this == _pCutscene->_pCurrentCameraCommand)
		_pCutscene->_pCurrentCameraCommand = nullptr;
}

void Cutscene::CameraCommand::Async_WhenLoaded(CSZ url, RcBlob blob)
{
	IO::StreamPtr sp(blob);
	_motion.Init();
	_motion.Deserialize(sp);
	_duration = _motion.GetDuration();
	if (_active)
		_pCutscene->OnNewActiveDuration(_delay + _duration);
}

void Cutscene::CameraCommand::Motion_OnTrigger(CSZ name, int state)
{
	if (!Str::StartsWith(name, "Camera."))
		return;
	if (state & 0x1)
	{
		_cameraName = name;
		_pCutscene->_camera.CutMotionBlur();
	}
}

// Cutscene::ConfigCommand:

Cutscene::PCommand Cutscene::ConfigCommand::Make()
{
	return new ConfigCommand;
}

void Cutscene::ConfigCommand::Parse(pugi::xml_node node)
{
	Command::Parse(node);

	_tag = node.attribute("tag").as_int(_tag);
	_callOnEnd = node.attribute("callOnEnd").as_bool(_callOnEnd);
	_skipHere = node.attribute("skipHere").as_bool(_skipHere);
}

void Cutscene::ConfigCommand::OnBegin()
{
	if (_callOnEnd)
	{
		if (_pCutscene->_fnOnEnd)
			_pCutscene->_fnOnEnd(_tag); // Tag 1 and above means called from config command.
	}
}

// Cutscene::FadeCommand:

Cutscene::PCommand Cutscene::FadeCommand::Make()
{
	return new FadeCommand;
}

void Cutscene::FadeCommand::Parse(pugi::xml_node node)
{
	Command::Parse(node);

	_from.FromColorString(node.attribute("from").value());
	_to.FromColorString(node.attribute("to").value());
	_easing = Math::EasingFromString(node.attribute("easing").value());
	_skipHere = node.attribute("skipHere").as_bool(_skipHere);
}

bool Cutscene::FadeCommand::DrawOverlay()
{
	if (IsDelayed())
		return false;

	VERUS_QREF_RENDERER;
	VERUS_QREF_VM;

	const float ratio = Math::Clamp<float>(GetTime() / _duration, 0, 1);
	const Vector4 color = VMath::lerp(Math::ApplyEasing(_easing, ratio), _from, _to);

	auto cb = renderer.GetCommandBuffer();
	auto shader = vm.GetShader();

	vm.GetUbGui()._matW = Transform3::UniformBufferFormatIdentity();
	vm.GetUbGuiFS()._color = color.GLM();

	vm.BindPipeline(GUI::ViewManager::PIPE_SOLID_COLOR, cb);
	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, vm.GetDefaultComplexSetHandle());
	shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	return true;
}

// Cutscene::MotionCommand:

Cutscene::PCommand Cutscene::MotionCommand::Make()
{
	return new MotionCommand;
}

bool Cutscene::MotionCommand::Update()
{
	if (IsDelayed() || FLT_MAX == _duration)
		return true;

	return true;
}

void Cutscene::MotionCommand::Motion_OnTrigger(CSZ name, int state)
{
}

// Cutscene::SoundCommand:

Cutscene::PCommand Cutscene::SoundCommand::Make()
{
	return new SoundCommand;
}

void Cutscene::SoundCommand::Parse(pugi::xml_node node)
{
	Command::Parse(node);

	_gain = node.attribute("gain").as_float(_gain);
	_pitch = node.attribute("pitch").as_float(_pitch);
	_duration = FLT_MAX;
	_sound.Init(_C(_url));
}

bool Cutscene::SoundCommand::Update()
{
	if (IsDelayed())
		return true;

	if (FLT_MAX == _duration && _sound->IsLoaded())
	{
		_duration = _sound->GetLength();
		_pCutscene->OnNewActiveDuration(_delay + _duration);
	}

	const float time = GetTime();
	if (time >= _duration)
	{
		OnEnd();
		return false;
	}

	if (!_source)
	{
		_sound->NewSource(&_source)->Play();
		_source->SetGain(_gain);
		_source->SetPitch(_pitch);
	}

	return true;
}

void Cutscene::SoundCommand::OnEnd()
{
	Command::OnEnd();

	if (_source)
		_source->Stop();
}

// Cutscene:

Cutscene::Cutscene()
{
}

Cutscene::~Cutscene()
{
	Done();
}

void Cutscene::Init()
{
	VERUS_INIT();

	RegisterCommand("barrier", &BarrierCommand::Make);
	RegisterCommand("camera", &CameraCommand::Make);
	RegisterCommand("config", &ConfigCommand::Make);
	RegisterCommand("fade", &FadeCommand::Make);
	RegisterCommand("motion", &MotionCommand::Make);
	RegisterCommand("sound", &SoundCommand::Make);

	OnWindowSizeChanged();
}

void Cutscene::Done()
{
	DeleteCommands();
	VERUS_DONE(Cutscene);
}

void Cutscene::Next(CSZ url, bool preload)
{
	_url = url;
	if (preload)
		Load(_C(_url));
}

void Cutscene::Load(CSZ url)
{
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData);

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
	pugi::xml_node root = doc.first_child();

	_interactive = root.attribute("interactive").as_bool(_interactive);

	DeleteCommands();
	_vCommands.reserve(16);
	for (auto node : root.children())
	{
		CSZ commandType = node.name();
		PCommand pCommand = CreateCommand(commandType);
		if (pCommand)
		{
			pCommand->SetCutscene(this);
			pCommand->Parse(node);
			_vCommands.push_back(pCommand);
		}
		else
			throw VERUS_RECOVERABLE << "CreateCommand(), type=" << commandType;
	}
}

void Cutscene::OnBegin()
{
	if (_vCommands.empty())
		Load(_C(_url));
}

void Cutscene::OnEnd()
{
	if (_fnOnEnd)
		_fnOnEnd(0); // Tag 0 means whole cutscene.
}

bool Cutscene::CanAutoPop()
{
	return _beginIndex >= _vCommands.size(); // No more commands.
}

void Cutscene::RegisterCommand(CSZ type, PFNCREATOR pCreator)
{
	_mapCreators[type] = pCreator;
}

Cutscene::PCommand Cutscene::CreateCommand(CSZ type)
{
	VERUS_IF_FOUND_IN(TMapCreators, _mapCreators, type, it)
		return it->second();
	return nullptr;
}

void Cutscene::DeleteCommands()
{
	for (auto& p : _vCommands)
		delete p;
	_vCommands.clear();
}

void Cutscene::Skip()
{
	PCommand pSkipToDelayed = nullptr;
	for (int i = _beginIndex; i < _endIndex; ++i)
	{
		if (_vCommands[i]->IsActive() &&
			_vCommands[i]->SkipHere() &&
			_vCommands[i]->IsDelayed() &&
			_vCommands[i]->GetDelay() != FLT_MAX)
		{
			pSkipToDelayed = _vCommands[i];
			break;
		}
	}

	if (pSkipToDelayed) // Soft skip using fade command:
	{
		const float adjustDelayBy = pSkipToDelayed->GetTime();
		pSkipToDelayed->AdjustDelayBy(adjustDelayBy);
		const float delay = pSkipToDelayed->GetDelay();
		const float duration = pSkipToDelayed->GetDuration();
		const float maxActiveDuration = delay + duration;
		for (int i = _beginIndex; i < _endIndex; ++i)
		{
			if (_vCommands[i] != pSkipToDelayed && _vCommands[i]->IsActive())
				_vCommands[i]->LimitActiveDuration(maxActiveDuration);
		}
	}
	else // Hard skip using config command:
	{
		// Stop all current commands:
		for (int i = _beginIndex; i < _endIndex; ++i)
		{
			if (_vCommands[i]->IsActive())
				_vCommands[i]->OnEnd();
		}
		// Find where to skip:
		while (_endIndex < _vCommands.size())
		{
			if (_vCommands[_endIndex]->SkipHere())
				break;
			_endIndex++;
		}
		_beginIndex = _endIndex;
		_timeSinceBarrier = 0;
	}
}

bool Cutscene::SkipBarrier()
{
	VERUS_RT_ASSERT(_beginIndex < _vCommands.size());
	if (!_vCommands[_beginIndex]->IsBarrier())
		return false;

	_beginIndex++;
	_endIndex++;
	_timeSinceBarrier = 0;

	return true;
}

bool Cutscene::RunParallelCommands()
{
	VERUS_RT_ASSERT(_beginIndex < _vCommands.size());
	if (_vCommands[_beginIndex]->IsBarrier())
		return false;

	while (_endIndex < _vCommands.size())
	{
		if (_vCommands[_endIndex]->IsBarrier())
			break;
		_vCommands[_endIndex++]->OnBegin();
	}

	return _beginIndex != _endIndex;
}

Continue Cutscene::Update()
{
	VERUS_QREF_TIMER;

	_timeSinceBarrier += dt;

	bool hasActiveCommands = false;
	do
	{
		if (_beginIndex >= _vCommands.size()) // No more commands?
			break;

		if (_beginIndex == _endIndex) // At barrier?
		{
			SkipBarrier();
			RunParallelCommands();

			// Compute duration for this group:
			float maxActiveDuration = 0;
			for (int i = _beginIndex; i < _endIndex; ++i)
			{
				const float delay = _vCommands[i]->GetDelay();
				const float duration = _vCommands[i]->GetDuration();
				if (delay != FLT_MAX && duration != FLT_MAX)
				{
					const float activeDuration = delay + duration;
					if (i + 1 < _vCommands.size())
						_vCommands[i + 1]->UpdatePrevDelay(activeDuration);
					maxActiveDuration = Math::Max(maxActiveDuration, activeDuration);
				}
			}
			if (maxActiveDuration > 0)
			{
				for (int i = _beginIndex; i < _endIndex; ++i)
					_vCommands[i]->UpdateEndDelay(maxActiveDuration);
			}
		}

		for (int i = _beginIndex; i < _endIndex; ++i)
		{
			if (_vCommands[i]->IsActive() && _vCommands[i]->Update())
				hasActiveCommands = true;
		}

		if (!hasActiveCommands)
			_beginIndex = _endIndex; // Go to next barrier.
	} while (!hasActiveCommands); // Don't return while at barrier.

	return Continue::yes;
}

Continue Cutscene::DrawOverlay()
{
	for (int i = _beginIndex; i < _endIndex; ++i)
	{
		if (_vCommands[i]->IsActive())
			_vCommands[i]->DrawOverlay();
	}
	return Continue::yes;
}

bool Cutscene::IsInputEnabled()
{
	return _interactive;
}

Continue Cutscene::OnMouseMove(float x, float y)
{
	return _interactive ? Continue::yes : Continue::no;
}

Scene::PMainCamera Cutscene::GetMainCamera()
{
	return _pCurrentCameraCommand ? &_camera : nullptr;
}

void Cutscene::OnWindowSizeChanged()
{
	VERUS_QREF_RENDERER;
	_camera.SetAspectRatio(renderer.GetSwapChainAspectRatio());
	_camera.Update();
}

void Cutscene::OnNewActiveDuration(float newActiveDuration)
{
	VERUS_RT_ASSERT(newActiveDuration != 0 && newActiveDuration != FLT_MAX);
	for (int i = _beginIndex; i < _endIndex; ++i)
	{
		if (i + 1 < _vCommands.size())
			_vCommands[i + 1]->UpdatePrevDelay(newActiveDuration);
		_vCommands[i]->UpdateEndDelay(newActiveDuration);
	}
}
