#pragma once

#include <windows.h> 
#include <chrono>
#include <atomic>
#include <sal.h>

class GameTimer
{
public:
	using Timer = std::chrono::steady_clock;
	using TimePoint = Timer::time_point;

	GameTimer();

	GameTimer(_In_ const GameTimer&) = delete;
	GameTimer(_Inout_ GameTimer&&) = delete;

	GameTimer& operator=(_In_ const GameTimer&) = delete;
	GameTimer& operator=(_Inout_ GameTimer&&) = delete;

	//~ GameTimer features
	void ResetTime();

	// returns delta time in seconds
	_NODISCARD _Check_return_ float Tick();
	_NODISCARD _Check_return_ float TimeElapsed() const; // total time in secs
	_NODISCARD _Check_return_ float DeltaTime() const;
private:
	std::atomic<TimePoint> m_timeStart;
	std::atomic<TimePoint> m_timeLastTick;
};
