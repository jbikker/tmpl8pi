#include "template.h"

Surface test( "assets/spec.jpg" );

int xpos = 50, ypos = 50;

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void Game::Init()
{
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float deltaTime )
{
	if (keystate[KEY_ESC]) exit( 0 );
	if (keystate[KEY_LEFT]) xpos--;
	if (keystate[KEY_RIGHT]) xpos++;
	if (keystate[KEY_UP]) ypos--;
	if (keystate[KEY_DOWN]) ypos++;
	screen->Clear( 0 );
	screen->Print( "hello world", 2, 2, 0xffffff );
	test.CopyTo( screen, xpos, ypos );
	screen->pixels[mousePos.x + mousePos.y * screen->width] = 0xffffffff;
}