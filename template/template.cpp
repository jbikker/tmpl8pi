// Template, IGAD version 3, Raspberry PI 4 version
// Get the latest version from: https://github.com/jbikker/tmpl8pi
// IGAD/NHTV/BUAS/UU - Jacco Bikker - 2006-2023

#define STB_IMAGE_IMPLEMENTATION
#include "template.h"

using namespace Tmpl8;

// Enable usage of dedicated GPUs in notebooks
// Note: this does cause the linker to produce a .lib and .exp file;
// see http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
#if WINBUILD
#pragma comment( linker, "/subsystem:windows /ENTRY:mainCRTStartup" )
/* extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

extern "C"
{
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
} */
#endif

Game* game;

// Get us to the correct working folder when running from vs
// ----------------------------------------------------------------------------
void FixWorkingFolder()
{
	static bool fixed = false;
	if (fixed) return;
	FILE* f = fopen( "assets/font.png", "rb" );
	if (f) fclose( f ); /* if this worked, we're already in the right folder */ else chdir( "../../.." );
	fixed = true;
}

// Helper functions
// ----------------------------------------------------------------------------

bool FileExists( const char* f )
{
	ifstream s( f );
	return s.good();
}

bool RemoveFile( const char* f )
{
	if (!FileExists( f )) return false;
	return !remove( f );
}

uint FileSize( string filename )
{
	ifstream s( filename );
	return s.good();
}

string TextFileRead( const char* _File )
{
	ifstream s( _File );
	string str( (istreambuf_iterator<char>( s )), istreambuf_iterator<char>() );
	s.close();
	return str;
}

void FatalError( const char* fmt, ... )
{
	char t[16384];
	va_list args;
	va_start( args, fmt );
	vsnprintf( t, sizeof( t ), fmt, args );
	va_end( args );
	printf( t );
	exit( 0 );
}

void CheckEGL( EGLBoolean result, const char* file, const uint line )
{
	if (result == EGL_TRUE) return;
	GLint error = glGetError();
	if (error == GL_INVALID_ENUM) FATALERROR( "EGL error: invalid enum.\n%s, line %i", file, line );
	if (error == GL_INVALID_VALUE) FATALERROR( "EGL error: invalid value.\n%s, line %i", file, line );
	if (error == GL_INVALID_OPERATION) FATALERROR( "EGL error: invalid operation.\n%s, line %i", file, line );
	if (error == GL_OUT_OF_MEMORY) FATALERROR( "EGL error: out of memory.\n%s, line %i", file, line );
	if (error == EGL_BAD_DISPLAY) FATALERROR( "EGL error: bad display.\n%s, line %i", file, line );
	if (error == EGL_BAD_ATTRIBUTE) FATALERROR( "EGL error: bad attribute.\n%s, line %i", file, line );
	if (error == EGL_NOT_INITIALIZED) FATALERROR( "EGL error: not initialized.\n%s, line %i", file, line );
	if (error == EGL_BAD_PARAMETER) FATALERROR( "EGL error: bad parameter.\n%s, line %i", file, line );
	FATALERROR( "EGL error: unknown error.\n%s, line %i", file, line );
}

#define CHECK_EGL( x ) CheckEGL( x, __FILE__, __LINE__ )

// Minimal X11 input manager
// ----------------------------------------------------------------------------

static Display* x11Display;
static long unsigned int x11Window;
static EGLContext eglContext;
static EGLConfig eglConfig;
static EGLDisplay eglDisplay;
static EGLSurface eglSurface;
static int* ks = 0;
static int device = -1;

void* InputHandlerThread( void* x )
{
	device = open( "/dev/input/event0", O_RDONLY );
	if (device < 0) printf( "could not open keyboard.\n" ); else while (1)
	{
		struct input_event e[64];
		ssize_t eventsSize = read( device, e, sizeof( input_event ) * 64 );
		int events = (int)(eventsSize / sizeof( input_event ));
		for (int i = 0; i < events; i++)
		{
			if (e[i].type == EV_KEY /* keyboard */)
			{
				if (e[i].code == BTN_LEFT) { /* mouse button */ }
				else if (e[i].code == BTN_MIDDLE) { /* mouse button */ }
				else if (e[i].code == BTN_RIGHT) { /* mouse button */ }
				else if (e[i].value == 2) { /* ignore key repeat */ }
				else if (e[i].value == 1 /* down */) ks[e[i].code] = 1;
				else if (e[i].value == 0 /* up */) ks[e[i].code] = 0;
			}
			else if (e[i].type == EV_REL /* mouse; see input-event-codes.h for others */)
			{
				if (e[i].code == REL_WHEEL) { /* check e[i].value */ }
			}
		}
	}
	return 0;
}
void GetMousePos( int& childx, int& childy )
{
	int rootx, rooty;
	uint mask;
	Window w1, w2;
	XQueryPointer( x11Display, x11Window, &w1, &w2, &rootx, &rooty, &childx, &childy, &mask );
}

// EGL initialization; 
// heavily based on code by Brian Beuken
// ----------------------------------------------------------------------------

void InitEGL()
{
	// open display
	if (!(x11Display = XOpenDisplay( NULL ))) FatalError( "Could not open display" );
	x11Window = DefaultRootWindow( x11Display );
	// set window attributes
	XSetWindowAttributes windowAttributes{};
	windowAttributes.event_mask = ExposureMask | PointerMotionMask | KeyPressMask | KeyReleaseMask;
	windowAttributes.background_pixmap = None;
	windowAttributes.background_pixel = 0;
	windowAttributes.border_pixel = 0;
	windowAttributes.override_redirect = true;
	// create window
	x11Window = XCreateWindow( x11Display, x11Window, 0, 0, SCRWIDTH, SCRHEIGHT, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &windowAttributes );
	if (!x11Window) FatalError( "Could not create window" );
	// show the window
	XMapWindow( x11Display, x11Window );
	XStoreName( x11Display, x11Window, "pi4 template" );
	// get EGL display
	if (!(eglDisplay = eglGetDisplay( (EGLNativeDisplayType)x11Display ))) FatalError( "Could not get EGL display" );
	// init EGL
	EGLint majorVersion = 0, minorVersion = 0;
	if (!eglInitialize( eglDisplay, &majorVersion, &minorVersion )) FatalError( "Could not initialize EGL: %i", eglGetError() );
	// get EGL frame buffer configs for display
	EGLint numConfigs;
	eglGetConfigs( eglDisplay, NULL, 0, &numConfigs );
	// choose EGL frame buffer configuration
	static const EGLint EGL_DISPLAY_ATTRIBUTE_LIST[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE,	8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_CONFORMANT, EGL_OPENGL_ES3_BIT_KHR,
		EGL_SAMPLE_BUFFERS, 0, // no MSAA
		EGL_SAMPLES, 1, // or 4, for MSAA
		EGL_NONE
	};
	eglChooseConfig( eglDisplay, EGL_DISPLAY_ATTRIBUTE_LIST, &eglConfig, 1, &numConfigs );
	// create surface to display graphics on
	eglSurface = eglCreateWindowSurface( eglDisplay, eglConfig, (EGLNativeWindowType)x11Window, NULL );
	// create EGL rendering context
	static const EGLint GLES3_ATTRIBUTE_LIST[] =
	{
		EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
		EGL_CONTEXT_MINOR_VERSION_KHR, 1,
		EGL_NONE, EGL_NONE
	};
	eglContext = eglCreateContext( eglDisplay, eglConfig, NULL, GLES3_ATTRIBUTE_LIST );
	if (eglContext == EGL_NO_CONTEXT) FatalError( "Could not create context: %i", eglGetError() );
	// all done
	eglMakeCurrent( eglDisplay, eglSurface, eglSurface, eglContext );
	printf( "Initialized with major verision: %i, minor version: %i\n", majorVersion, minorVersion );
	printf( "This GPU supports: %s\n", glGetString( GL_VERSION ) );
	// prepare egl state
	eglSwapInterval( eglDisplay, 0 );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( 1 );
	glDepthRangef( 0.0f, 1.0f );
	glClearDepthf( 1.0f );
	glViewport( 0, 0, SCRWIDTH, SCRHEIGHT );
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glCullFace( GL_BACK );
}

void closeEGL()
{
	XDestroyWindow( x11Display, x11Window );
	XFree( ScreenOfDisplay( x11Display, 0 ) );
	XCloseDisplay( x11Display );
}

// application entry point
// ----------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
	setenv( "DISPLAY", ":0", 1 );
	InitEGL();
	GLTexture* renderTarget = new GLTexture( SCRWIDTH, SCRHEIGHT, GLTexture::INTTARGET );
#if WINBUILD
	Shader* shader = new Shader(
		"#version 330\nin vec4 p;\nin vec2 t;out vec2 u;void main(){u=t;gl_Position=p;}",
		"#version 330\nuniform sampler2D c;in vec2 u;out vec4 f;void main(){f=sqrt(texture(c,u));}", true );
#else
	Shader* shader = new Shader(
		"precision mediump float;attribute vec3 p;varying vec2 u;void main(){u=vec2(p.x*0.5+0.5,0.5-p.y*0.5);gl_Position=vec4(p,1);}",
		"precision mediump float;varying vec2 u;uniform sampler2D c;void main(){gl_FragColor=texture2D(c,u).zyxw;}", true );
#endif
	FixWorkingFolder();
	Surface screen( SCRWIDTH, SCRHEIGHT );
	screen.Clear( 0 );
	glViewport( 0, 0, SCRWIDTH, SCRHEIGHT );
	game = new Game();
	game->SetTarget( &screen );
	game->Init();
	glViewport( 0, 0, SCRWIDTH, SCRHEIGHT );
	eglSwapInterval( eglDisplay, 0 );
	ks = game->keystate;
	pthread_t dummy;
	pthread_create( &dummy, 0, InputHandlerThread, 0 );
	while (1)
	{
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		GetMousePos( game->mousePos.x, game->mousePos.y );
		game->Tick( 0 /* no timing yet */ );
		renderTarget->CopyFrom( &screen );
		shader->Bind();
		shader->SetInputTexture( 0, "c", renderTarget );
		DrawQuad();
		shader->Unbind();
		glFlush();
		eglSwapBuffers( eglDisplay, eglSurface );
	}
}