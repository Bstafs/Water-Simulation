#pragma once

#include <functional>
#include <d3d11_1.h>

constexpr double FPS_60 = 1.0 / 60.0;


	class Timestep
	{
	public:
		Timestep(std::function<void(double dt)> doTick);
		~Timestep();

		void CalculateTimestep();

		double DeltaTime;

		const double GetDeltaTime() { return DeltaTime; }

	public:
		ULONGLONG _lastTime;
		double _timer;
		double _accumulator;
		ULONGLONG _nowTime;
		int _frames;
		int _updates;
		std::function<void(double dt)> _doTick;
	};

