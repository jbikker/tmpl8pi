#pragma once

namespace Tmpl8
{

class Game
{
public:
	// game flow methods
	void Init();
	void Tick( float deltaTime );
	void Shutdown() { /* implement if you want to do something on exit */ }
	void SetTarget( Surface* s ) { screen = s; }
	// data members
	Surface* screen;
	int keystate[256]; // see input-event-codes.h
	int2 mousePos;
};

} // namespace Tmpl8