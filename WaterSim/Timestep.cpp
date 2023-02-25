#include "Timestep.h"
#include "Timestep.h"

Timestep::Timestep(std::function<void(double dt)> doTick) : _doTick(doTick)
{
	_lastTime = GetTickCount64();
	_timer = _lastTime;
	_accumulator = 0;
	_nowTime = 0;
	_frames = 0;
	_updates = 0;
	DeltaTime = 0;
}

Timestep::~Timestep()
{
}

void Timestep::CalculateTimestep()
{
	_nowTime = GetTickCount64();
	_accumulator += ((_nowTime - _lastTime) / FPS_60);
	_lastTime = _nowTime;

	while (_accumulator >= 1.0)
	{
		DeltaTime = FPS_60;
		_doTick(DeltaTime);
		_updates++;
		_accumulator--;
	}

	
	// Reset after one second
	if (GetTickCount64() - _timer > 1.0)
	{
		_timer++;
		//std::cout << "UPS: " << _updates << std::endl;
		_updates = 0;
		_frames = 0;
	}

}
