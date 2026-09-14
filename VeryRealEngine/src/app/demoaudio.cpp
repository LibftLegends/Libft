#include "demoaudio.hpp"

namespace vre
{
DemoAudio::DemoAudio() : _audio(nullptr), _available(false),
	_ambient_sound(SoundHandle::invalid()),
	_click_sound(SoundHandle::invalid()),
	_door_creak_sound(SoundHandle::invalid())
{
}

DemoAudio::~DemoAudio()
{
	destroy();
}

bool DemoAudio::initialize()
{
	// Sound system (bonus, Chapter VII): a looping ambient room hum plus
	// one-shot effects on the door and light switch. A false return here
	// (no device, e.g. a headless/sandboxed session) is not fatal — see
	// audio_system.hpp's doc comment — the demo just runs silently rather
	// than aborting.
	_audio = AudioSystem::create();
	_available = _audio->initialize();
	if (_available)
	{
		_ambient_sound = _audio->load_sound("assets/sounds/ambient_hum.wav");
		_click_sound = _audio->load_sound("assets/sounds/click.wav");
		_door_creak_sound = _audio->load_sound(
				"assets/sounds/door_creak.wav");
		if (_ambient_sound.is_valid())
			_audio->play(_ambient_sound, /*loop=*/true, /*volume=*/0.35f);
	}
	return (_available);
}

void DemoAudio::destroy()
{
	if (_audio == nullptr)
		return ;
	_audio->destroy();
	delete _audio;
	_audio = nullptr;
}

void DemoAudio::play_door_creak()
{
	if (_available && _door_creak_sound.is_valid())
		_audio->play(_door_creak_sound, /*loop=*/false, /*volume=*/0.8f);
}

void DemoAudio::play_click()
{
	if (_available && _click_sound.is_valid())
		_audio->play(_click_sound, /*loop=*/false, /*volume=*/0.6f);
}

} // namespace vre
