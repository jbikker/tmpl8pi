#include "template.h"

Surface test( "assets/spec.jpg" );

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
	screen->Clear( 0 );
	screen->Print( "hello world", 2, 2, 0xffffff );
	test.CopyTo( screen, 50, 50 );
}