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

// RNG - Marsaglia's xor32
// ----------------------------------------------------------------------------

static uint seed = 0x12345678;
uint RandomUInt()
{
	seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
	return seed;
}
float RandomFloat() { return (float)RandomUInt() * 2.3283064365387e-10f; }
float Rand( float range ) { return RandomFloat() * range; }
// local seed
uint RandomUInt( uint& seed )
{
	seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
	return seed;
}
float RandomFloat( uint& seed ) { return (float)RandomUInt( seed ) * 2.3283064365387e-10f; }

// Perlin noise implementation
// https://stackoverflow.com/questions/29711668/perlin-noise-generation
// ----------------------------------------------------------------------------

static int numOctaves = 7, primeIndex = 0;
static float persistence = 0.5f;
static int primes[10][3] = {
	{ 995615039, 600173719, 701464987 }, { 831731269, 162318869, 136250887 }, { 174329291, 946737083, 245679977 },
	{ 362489573, 795918041, 350777237 }, { 457025711, 880830799, 909678923 }, { 787070341, 177340217, 593320781 },
	{ 405493717, 291031019, 391950901 }, { 458904767, 676625681, 424452397 }, { 531736441, 939683957, 810651871 },
	{ 997169939, 842027887, 423882827 }
};
static float Noise( const int i, const int x, const int y )
{
	int n = x + y * 57;
	n = (n << 13) ^ n;
	const int a = primes[i][0], b = primes[i][1], c = primes[i][2];
	const int t = (n * (n * n * a + b) + c) & 0x7fffffff;
	return 1.0f - (float)t / 1073741824.0f;
}
static float SmoothedNoise( const int i, const int x, const int y )
{
	const float corners = (Noise( i, x - 1, y - 1 ) + Noise( i, x + 1, y - 1 ) + Noise( i, x - 1, y + 1 ) + Noise( i, x + 1, y + 1 )) / 16;
	const float sides = (Noise( i, x - 1, y ) + Noise( i, x + 1, y ) + Noise( i, x, y - 1 ) + Noise( i, x, y + 1 )) / 8;
	const float center = Noise( i, x, y ) / 4;
	return corners + sides + center;
}
static float Interpolate( const float a, const float b, const float x )
{
	const float ft = x * 3.1415927f, f = (1 - cosf( ft )) * 0.5f;
	return a * (1 - f) + b * f;
}
static float InterpolatedNoise( const int i, const float x, const float y )
{
	const int integer_X = (int)x, integer_Y = (int)y;
	const float fractional_X = x - (float)integer_X, fractional_Y = y - (float)integer_Y;
	const float v1 = SmoothedNoise( i, integer_X, integer_Y );
	const float v2 = SmoothedNoise( i, integer_X + 1, integer_Y );
	const float v3 = SmoothedNoise( i, integer_X, integer_Y + 1 );
	const float v4 = SmoothedNoise( i, integer_X + 1, integer_Y + 1 );
	const float i1 = Interpolate( v1, v2, fractional_X );
	const float i2 = Interpolate( v3, v4, fractional_X );
	return Interpolate( i1, i2, fractional_Y );
}
float noise2D( const float x, const float y )
{
	float total = 0, frequency = (float)(2 << numOctaves), amplitude = 1;
	for (int i = 0; i < numOctaves; ++i)
	{
		frequency /= 2, amplitude *= persistence;
		total += InterpolatedNoise( (primeIndex + i) % 10, x / frequency, y / frequency ) * amplitude;
	}
	return total / frequency;
}

// Math implementations
// ----------------------------------------------------------------------------

mat4 operator*( const mat4& a, const mat4& b )
{
	mat4 r;
	for (uint i = 0; i < 16; i += 4)
		for (uint j = 0; j < 4; ++j)
		{
			r[i + j] =
				(a.cell[i + 0] * b.cell[j + 0]) +
				(a.cell[i + 1] * b.cell[j + 4]) +
				(a.cell[i + 2] * b.cell[j + 8]) +
				(a.cell[i + 3] * b.cell[j + 12]);
		}
	return r;
}
mat4 operator*( const mat4& a, const float s )
{
	mat4 r;
	for (uint i = 0; i < 16; i += 4) r.cell[i] = a.cell[i] * s;
	return r;
}
mat4 operator*( const float s, const mat4& a )
{
	mat4 r;
	for (uint i = 0; i < 16; i++) r.cell[i] = a.cell[i] * s;
	return r;
}
mat4 operator+( const mat4& a, const mat4& b )
{
	mat4 r;
	for (uint i = 0; i < 16; i += 4) r.cell[i] = a.cell[i] + b.cell[i];
	return r;
}
bool operator==( const mat4& a, const mat4& b )
{
	for (uint i = 0; i < 16; i++)
		if (a.cell[i] != b.cell[i]) return false;
	return true;
}
bool operator!=( const mat4& a, const mat4& b ) { return !(a == b); }
float4 operator*( const mat4& a, const float4& b )
{
	return make_float4( a.cell[0] * b.x + a.cell[1] * b.y + a.cell[2] * b.z + a.cell[3] * b.w,
		a.cell[4] * b.x + a.cell[5] * b.y + a.cell[6] * b.z + a.cell[7] * b.w,
		a.cell[8] * b.x + a.cell[9] * b.y + a.cell[10] * b.z + a.cell[11] * b.w,
		a.cell[12] * b.x + a.cell[13] * b.y + a.cell[14] * b.z + a.cell[15] * b.w );
}
float4 operator*( const float4& b, const mat4& a )
{
	return make_float4( a.cell[0] * b.x + a.cell[1] * b.y + a.cell[2] * b.z + a.cell[3] * b.w,
		a.cell[4] * b.x + a.cell[5] * b.y + a.cell[6] * b.z + a.cell[7] * b.w,
		a.cell[8] * b.x + a.cell[9] * b.y + a.cell[10] * b.z + a.cell[11] * b.w,
		a.cell[12] * b.x + a.cell[13] * b.y + a.cell[14] * b.z + a.cell[15] * b.w );
}
float3 TransformPosition( const float3& a, const mat4& M )
{
	return make_float3( make_float4( a, 1 ) * M );
}
float3 TransformVector( const float3& a, const mat4& M )
{
	return make_float3( make_float4( a, 0 ) * M );
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
	sleep( 1500 );
	while (1) exit( 0 );
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

// EGL initialization; 
// heavily based on code by Brian Beuken
// ----------------------------------------------------------------------------

static int scrwidth = SCRWIDTH, scrheight = SCRHEIGHT;
static EGLDisplay egl_display;
static EGLSurface egl_surface;

void InitEGL()
{
	Display* x_display = XOpenDisplay( NULL );
	if (x_display == NULL) FatalError( "Can't open an XDisplay." );
	printf( "we got an XDisplay\n" );
	// detect screen res; be sure to have libXrandr installed: sudo apt-get install libxrandr-dev
	static int num_sizes;
	XRRScreenSize* xrrs = XRRSizes( x_display, 0, &num_sizes );
	// https://www.x.org/releases/X11R7.5/doc/man/man3/Xrandr.3.html: first entry is preferred size
	if (num_sizes == 0)
	{
		// headless - if SCRWIDTH / SCRHEIGHT are correct, the app will work as intended.
		scrwidth = SCRWIDTH;
		scrheight = SCRHEIGHT;
	}
	else
	{
		scrwidth = xrrs[0].width;
		scrheight = xrrs[0].height;
	}
	// create application window
	Window root = DefaultRootWindow( x_display );
	XSetWindowAttributes swa, xattr;
	swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask | KeyReleaseMask;
	swa.background_pixmap = None;
	swa.background_pixel = swa.border_pixel = 0;
	swa.override_redirect = true;
	Window win = XCreateWindow( x_display, root, 0, 0, scrwidth, scrheight, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa );
	if (win == 0) FatalError( "Can't create a window." );
	printf( "we got an (Native) XWindow\n" );
	EGLNativeWindowType egl_window = (EGLNativeWindowType)win;
	XSelectInput( x_display, win, KeyPressMask | KeyReleaseMask );
	xattr.override_redirect = 1;
	XChangeWindowAttributes( x_display, win, CWOverrideRedirect, &xattr );
	XWMHints hints;
	hints.input = 1;
	hints.flags = InputHint;
	XSetWMHints( x_display, win, &hints );
	char* title = (char*)"Voxel World Template";
	// make the window visible on the screen
	XMapWindow( x_display, win );
	XStoreName( x_display, win, title );
	// get identifiers for the provided atom name strings
	Atom wm_state = XInternAtom( x_display, "_NET_WM_STATE", 0 );
	XEvent xev;
	memset( &xev, 0, sizeof( xev ) );
	xev.type = ClientMessage;
	xev.xclient.window = win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = 0;
	XSendEvent( x_display, DefaultRootWindow( x_display ), 0, SubstructureNotifyMask, &xev );
	// get Display	
	egl_display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	if (egl_display == EGL_NO_DISPLAY) FatalError( "GetDisplay error." );
	printf( "we got an EGLDisplay\n" );
	// initialize EGL
	EGLint majorVersion, minorVersion, numConfigs;
	CHECK_EGL( eglInitialize( egl_display, &majorVersion, &minorVersion ) );
	printf( "we initialised EGL\n" );
	// get configs
	CHECK_EGL( eglGetConfigs( egl_display, NULL, 0, &numConfigs ) );
	printf( "we got %i Configs\n", numConfigs );
	// choose config
	EGLConfig config;
	const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_SURFACE_TYPE,
		EGL_WINDOW_BIT, EGL_CONFORMANT, EGL_OPENGL_ES2_BIT, /* EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 1, */ EGL_NONE
	};
	const EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	CHECK_EGL( eglChooseConfig( egl_display, attribute_list, &config, 1, &numConfigs ) );
	printf( "we chose our config\n" );
	// create a GL context
	EGLContext egl_context = eglCreateContext( egl_display, config, EGL_NO_CONTEXT, context_attributes );
	if (egl_context == EGL_NO_CONTEXT) FATALERROR( "Could not create an EGL context." );
	printf( "Created a context ok\n" );
	// create a surface
	egl_surface = eglCreateWindowSurface( egl_display, config, egl_window, NULL );
	if (egl_surface == EGL_NO_SURFACE) FATALERROR( "Could not create a window surface." );
	printf( "we got a Surface\n" );
	// make the context current
	CHECK_EGL( eglMakeCurrent( egl_display, egl_surface, egl_surface, egl_context ) );
	eglSwapInterval( egl_display, 1 ); // 1 for vsync, 0 for none
	// see if we got some extensions that we need later
	char* extensions = (char*)glGetString( GL_EXTENSIONS );
	if (strstr( extensions, "GL_OES_EGL_image" )) printf( "We have the GL_OES_EGL_image extension.\n" );
	if (strstr( extensions, "GL_OES_EGL_image_external" )) printf( "We have the GL_OES_EGL_image_external extension.\n" );
	// some OpenGLES2.0 states that we might need
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( 1 );
	glDepthRangef( 0.0f, 1.0f );
	glClearDepthf( 1.0f );
	printf( "This SBC supports version %i.%i of EGL\n", majorVersion, minorVersion );
	printf( "This GPU supplied by  :%s\n", glGetString( GL_VENDOR ) );
	printf( "This GPU supports     :%s\n", glGetString( GL_VERSION ) );
	printf( "This GPU Renders with :%s\n", glGetString( GL_RENDERER ) );
	printf( "This GPU supports     :%s\n", glGetString( GL_SHADING_LANGUAGE_VERSION ) );
	printf( "This GPU supports these extensions	:%s\n", glGetString( GL_EXTENSIONS ) );
	glViewport( 0, 0, scrwidth, scrheight );
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glCullFace( GL_BACK );
	if (glGetError() == GL_NO_ERROR) return;
	FATALERROR( "EGL/OGL graphic init failed." );
}

void* InputManager::ProcessMouseThread( void* arg )
{
	InputManager* input = (InputManager*)arg;
	FILE* fmouse;
	fmouse = fopen( "/dev/input/mice", "r" );
	if (fmouse != NULL)
	{
		while (((InputManager*)arg)->mQuit == false) // so as long as mQuit is FALSE, this will endlessly loop
		{
			signed char b[3];
			fread( b, sizeof( char ), 3, fmouse );
			// if we do plan to scale, best make these into floats for greater precision before they are cast down to ints.
			float mousex = (float)b[1];
			float mousey = -(float)b[2];
			input->theMouse.relx = mousex;
			input->theMouse.rely = -mousey;
			input->theMouse.xpos += (int)(mousex / 1.0f); // 1.0 can be replaced by a scale factor (optional)
			if (input->theMouse.xpos > SCRWIDTH) input->theMouse.xpos = SCRWIDTH;
			if (input->theMouse.xpos < 0) input->theMouse.xpos = 0;
			input->theMouse.ypos += (int)(mousey / 1.0f);
			if (input->theMouse.ypos > SCRHEIGHT) input->theMouse.ypos = SCRHEIGHT;
			if (input->theMouse.ypos < 0) input->theMouse.ypos = 0;
			input->theMouse.leftButton = (b[0] & 1) > 0; // using a test( x >0 )  allows it to return and store a bool
			input->theMouse.midButton = (b[0] & 4) > 0;
			input->theMouse.rightButton = (b[0] & 2) > 0;
		}
		fclose( fmouse );
	}
	pthread_exit( NULL );
}

void* InputManager::ProcessKeyboardThread( void* arg )
{
	InputManager* input = (InputManager*)arg;
	FILE* fp;
	fp = fopen( input->kbd.c_str(), "r" ); // normal scanned keyboard
	printf( "Using keyboard string :%s\n ", input->kbd.c_str() );
	struct input_event ev;
	if (fp != NULL)
	{
		while (input->kQuit == false) // kQuit is set to false by the init
		{
			fread( &ev, sizeof( struct input_event ), 1, fp );
			if (ev.type == (__u16)EV_KEY)
			{
				input->keys[ev.code] = ev.value > 0 ? 1 : 0; // never gets here to give me key values
				input->keyPressed = true;
			}
			else input->keyPressed = false;
		}
		fclose( fp );
	}
	pthread_exit( NULL );
}

bool InputManager::Init()
{
	kQuit = mQuit = false;
	iterations = 0;
	// mice don't usually provide any issues
	int result = pthread_create( &threadMouse, NULL, &ProcessMouseThread, this ); // we send the Input class (this) as an argument to allow for easy cast ((Input*)arg)-> type access to the classes data. 
	if (result != 0) printf( "got an error\n" ); else printf( "mouse thread started\n" );
	if (!AreYouMyKeyboard()) printf( "Oh Bugger, we can't seen to find the keyboard\n" ); // go find an active keyboard
	result = pthread_create( &threadKeyboard, NULL, &ProcessKeyboardThread, this );
	if (result != 0) printf( "got an error\n" ); else printf( "Key thread started\n" );
	return true;
}

// tests for the keyboard, which can be on different events in Linux
// thanks to my student Petar Dimitrov for this improvement to the keyboard search systems
int InputManager::AreYouMyKeyboard()
{
	/*
	 Note linux machines may have their key and mouse event files access protected,
	 in which case open a command line terminal, and enter
	 sudo chmod  a+r /dev/input/ (assuming your input event files are there)
	 this is kinda frowned on by linux users, but I don't know a way to overcome this in code yet.
	 it may also be possible to get VisualGDB to execute the build as sudo for root access but I find that flakey
	*/
	// Some bluetooth keyboards are registered as "event-mouse".
	// If this is your case, then just change this variable to event-mouse.
	std::string pattern = "event-kbd"; //<-change to event-mouse if your BT keyboard is a "mouse" or test for a failure to find a kbd, then try as a mouse.
	std::string file = "";
	DIR* dir;
	struct dirent* ent;
	printf( "Checking for active keyboard\n" );
	if ((dir = opendir( "/dev/input/by-path/" )) != nullptr)
	{
		while ((ent = readdir( dir )) != nullptr)
		{
			fprintf( stdout, "%s\n", ent->d_name );
			file = std::string( ent->d_name );
			if (!file.compare( "." ) || !file.compare( ".." )) continue;
			if (file.substr( file.length() - pattern.length() ) == pattern)
			{
				kbd = "/dev/input/by-path/" + file;
				fprintf( stdout, "%s\n", kbd.c_str() );
				return true;
			}
		}
	}
	return false;
}

// OpenGL helper functions
// ----------------------------------------------------------------------------

void _CheckGL( const char* f, int l )
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
	{
		const char* errStr = "UNKNOWN ERROR";
		if (error == 0x500) errStr = "INVALID ENUM";
		else if (error == 0x502) errStr = "INVALID OPERATION";
		else if (error == 0x501) errStr = "INVALID VALUE";
		else if (error == 0x506) errStr = "INVALID FRAMEBUFFER OPERATION";
		FatalError( "GL error %d: %s at %s:%d\n", error, errStr, f, l );
	}
}

GLuint CreateVBO( const GLfloat* data, const uint size )
{
	GLuint id;
	glGenBuffers( 1, &id );
	glBindBuffer( GL_ARRAY_BUFFER, id );
	glBufferData( GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW );
	CheckGL();
	return id;
}

void BindVBO( const uint idx, const uint N, const GLuint id )
{
	glEnableVertexAttribArray( idx );
	glBindBuffer( GL_ARRAY_BUFFER, id );
	glVertexAttribPointer( idx, N, GL_FLOAT, GL_FALSE, 0, (void*)0 );
	CheckGL();
}

void CheckShader( GLuint shader, const char* vshader, const char* fshader )
{
	char buffer[1024];
	memset( buffer, 0, sizeof( buffer ) );
	GLsizei length = 0;
	int status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );
	if (status) return;
	glGetShaderInfoLog( shader, sizeof( buffer ), &length, buffer );
	CheckGL();
	FATALERROR( "Shader compile error:\n%s", buffer );
}

void CheckProgram( GLuint id, const char* vshader, const char* fshader )
{
	char buffer[1024];
	memset( buffer, 0, sizeof( buffer ) );
	GLsizei length = 0;
	glGetProgramInfoLog( id, sizeof( buffer ), &length, buffer );
	CheckGL();
	FATALERROR_IF( length > 0, "Shader link error:\n%s", buffer );
}

void DrawQuad()
{
#if WINBUILD
	// OpenGL 4.x path
	static GLuint vao = 0;
	if (!vao)
	{
		// generate buffers
		static const GLfloat verts[] = { -1, 1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, -1, 0, 1, -1, 0 };
		static const GLfloat uvdata[] = { 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
		GLuint vertexBuffer = CreateVBO( verts, sizeof( verts ) );
		GLuint UVBuffer = CreateVBO( uvdata, sizeof( uvdata ) );
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		BindVBO( 0, 3, vertexBuffer );
		BindVBO( 1, 2, UVBuffer );
		glBindVertexArray( 0 );
		CheckGL();
	}
	glBindVertexArray( vao );
	glDrawArrays( GL_TRIANGLES, 0, 6 );
	glBindVertexArray( 0 );
#else
	// OpenGL 3.x path
	static GLuint quadVerts = 0;
	if (!quadVerts)
	{
		// generate buffers
		static const GLfloat verts[] = { -1, 1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, -1, 0, 1, -1, 0 };
		glGenBuffers( 1, &quadVerts );
		glBindBuffer( GL_ARRAY_BUFFER, quadVerts );
		glBufferData( GL_ARRAY_BUFFER, sizeof( verts ), verts, GL_STATIC_DRAW );
	}
	glBindBuffer( GL_ARRAY_BUFFER, quadVerts );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 );
	glDrawArrays( GL_TRIANGLES, 0, 6 );
	glDisableVertexAttribArray( 0 );
#endif
}

// OpenGL texture wrapper class
// ----------------------------------------------------------------------------

GLTexture::GLTexture( uint w, uint h, uint type )
{
	width = w;
	height = h;
	glGenTextures( 1, &ID );
	glBindTexture( GL_TEXTURE_2D, ID );
	if (type == DEFAULT)
	{
		// regular texture
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	#if WINBUILD
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR ? /* why? forgot... */, GL_UNSIGNED_BYTE, 0 );
	#else
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0 );
	#endif
	}
	else if (type == INTTARGET)
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	#if WINBUILD
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
	#else
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 ); // TODO: also fine for WINBUILD?
	#endif
	}
	else /* type == FLOAT */
	{
	#if WINBUILD == 0
		FatalError( "Floating point textures not supported in OpenGL ES 3.x." );
	#else
		// floating point texture
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0 );
	#endif
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
	CheckGL();
}

GLTexture::~GLTexture()
{
	glDeleteTextures( 1, &ID );
	CheckGL();
}

void GLTexture::Bind( const uint slot )
{
	glActiveTexture( GL_TEXTURE0 + slot );
	glBindTexture( GL_TEXTURE_2D, ID );
	CheckGL();
}

void GLTexture::CopyFrom( Surface* src )
{
	glBindTexture( GL_TEXTURE_2D, ID );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, src->buffer );
	CheckGL();
}

// Shader class implementation
// ----------------------------------------------------------------------------

Shader::Shader( const char* vfile, const char* pfile, bool fromString )
{
	if (fromString) Compile( vfile, pfile ); else Init( vfile, pfile );
}

Shader::~Shader()
{
	glDetachShader( ID, pixel );
	glDetachShader( ID, vertex );
	glDeleteShader( pixel );
	glDeleteShader( vertex );
	glDeleteProgram( ID );
	CheckGL();
}

void Shader::Init( const char* vfile, const char* pfile )
{
	string vsText = TextFileRead( vfile );
	string fsText = TextFileRead( pfile );
	FATALERROR_IF( vsText.size() == 0, "File %s not found", vfile );
	FATALERROR_IF( fsText.size() == 0, "File %s not found", pfile );
	const char* vertexText = vsText.c_str();
	const char* fragmentText = fsText.c_str();
	Compile( vertexText, fragmentText );
}

void Shader::Compile( const char* vtext, const char* ftext )
{
	vertex = glCreateShader( GL_VERTEX_SHADER );
	pixel = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( vertex, 1, &vtext, 0 );
	glCompileShader( vertex );
	CheckShader( vertex, vtext, ftext );
	glShaderSource( pixel, 1, &ftext, 0 );
	glCompileShader( pixel );
	CheckShader( pixel, vtext, ftext );
	ID = glCreateProgram();
	glAttachShader( ID, vertex );
	glAttachShader( ID, pixel );
	glBindAttribLocation( ID, 0, "p" );
	glBindAttribLocation( ID, 1, "t" );
	glLinkProgram( ID );
	CheckProgram( ID, vtext, ftext );
	CheckGL();
}

void Shader::Bind()
{
	glUseProgram( ID );
	CheckGL();
}

void Shader::Unbind()
{
	glUseProgram( 0 );
	CheckGL();
}

void Shader::SetInputTexture( uint slot, const char* name, GLTexture* texture )
{
	glActiveTexture( GL_TEXTURE0 + slot );
	glBindTexture( GL_TEXTURE_2D, texture->ID );
	glUniform1i( glGetUniformLocation( ID, name ), slot );
	CheckGL();
}

void Shader::SetInputMatrix( const char* name, const mat4& matrix )
{
	const GLfloat* data = (const GLfloat*)&matrix;
	glUniformMatrix4fv( glGetUniformLocation( ID, name ), 1, GL_FALSE, data );
	CheckGL();
}

void Shader::SetFloat( const char* name, const float v )
{
	glUniform1f( glGetUniformLocation( ID, name ), v );
	CheckGL();
}

void Shader::SetInt( const char* name, const int v )
{
	glUniform1i( glGetUniformLocation( ID, name ), v );
	CheckGL();
}

// application entry point
// ----------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
	setenv( "DISPLAY", ":0", 1 );
	static InputManager* input;
	InitEGL();
	input = new InputManager();
	input->Init();
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
	while (1)
	{
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		game->Tick( 0 /* no timing yet */ );
		renderTarget->CopyFrom( &screen );
		shader->Bind();
		shader->SetInputTexture( 0, "c", renderTarget );
		DrawQuad();
		shader->Unbind();
		glFlush();
		eglSwapBuffers( egl_display, egl_surface );
	}
}

// surface implementation
// ----------------------------------------------------------------------------

static char s_Font[51][5][6];
static bool fontInitialized = false;
static int s_Transl[256];

Surface::Surface( int w, int h, uint* b ) : buffer( b ), width( w ), height( h ) {}
Surface::Surface( int w, int h ) : width( w ), height( h )
{
	buffer = (uint*)MALLOC64( w * h * sizeof( uint ) );
}
Surface::Surface( const char* file ) : buffer( 0 ), width( 0 ), height( 0 )
{
	FixWorkingFolder(); // needed for Surfaces declared at global scope
	FILE* f = fopen( file, "rb" );
	if (!f) FATALERROR( "File not found: %s", file );
	fclose( f );
	LoadImage( file );
}

void Surface::LoadImage( const char* file )
{
	int w, h, n;
	unsigned char* pixels = stbi_load( file, &w, &h, &n, 0 );
	if (!pixels) FatalError( "could not load file %s.\n", file );
	width = w;
	height = h;
	buffer = (uint*)MALLOC64( width * height * sizeof( uint ) );
	if (n == 4) memcpy( buffer, pixels, w * h * 4 ); else if (n == 3)
	{
		for( int s = w * h, i = 0; i < s; i++ )
		{
			const uchar r = pixels[i * 3 + 0];
			const uchar g = pixels[i * 3 + 1];
			const uchar b = pixels[i * 3 + 2];
			buffer[i] = (255 << 24) + (r << 16) + (g << 8) + b;
		}
	}
	else FatalError( "bad component count in image %s.\n", file );
	stbi_image_free( pixels );
}

Surface::~Surface()
{
	FREE64( buffer ); // let's hope we allocated this
}

void Surface::Clear( uint c )
{
	const int s = width * height;
	for (int i = 0; i < s; i++) buffer[i] = c;
}

void Surface::Print( const char* s, int x1, int y1, uint c )
{
	if (!fontInitialized)
	{
		InitCharset();
		fontInitialized = true;
	}
	uint* t = buffer + x1 + y1 * width;
	for (int i = 0; i < (int)(strlen( s )); i++, t += 6)
	{
		int pos = 0;
		if ((s[i] >= 'A') && (s[i] <= 'Z')) pos = s_Transl[(unsigned short)(s[i] - ('A' - 'a'))];
		else pos = s_Transl[(unsigned short)s[i]];
		uint* a = t;
		const char* u = (const char*)s_Font[pos];
		for (int v = 0; v < 5; v++, u++, a += width)
			for (int h = 0; h < 5; h++) if (*u++ == 'o') *(a + h) = c, * (a + h + width) = 0;
	}
}

void Surface::CopyTo( Surface* d, int x, int y )
{
	uint* dst = d->buffer, * src = buffer;
	if ((src) && (dst))
	{
		int srcwidth = width, srcheight = height;
		int dstwidth = d->width, dstheight = d->height;
		if ((srcwidth + x) > dstwidth) srcwidth = dstwidth - x;
		if ((srcheight + y) > dstheight) srcheight = dstheight - y;
		if (x < 0) src -= x, srcwidth += x, x = 0;
		if (y < 0) src -= y * srcwidth, srcheight += y, y = 0;
		if ((srcwidth > 0) && (srcheight > 0))
		{
			dst += x + dstwidth * y;
			for (int y = 0; y < srcheight; y++)
			{
				memcpy( dst, src, srcwidth * 4 );
				dst += dstwidth, src += srcwidth;
			}
		}
	}
}

void Surface::SetChar( int c, const char* c1, const char* c2, const char* c3, const char* c4, const char* c5 )
{
	strcpy( s_Font[c][0], c1 );
	strcpy( s_Font[c][1], c2 );
	strcpy( s_Font[c][2], c3 );
	strcpy( s_Font[c][3], c4 );
	strcpy( s_Font[c][4], c5 );
}

void Surface::InitCharset()
{
	SetChar( 0, ":ooo:", "o:::o", "ooooo", "o:::o", "o:::o" );
	SetChar( 1, "oooo:", "o:::o", "oooo:", "o:::o", "oooo:" );
	SetChar( 2, ":oooo", "o::::", "o::::", "o::::", ":oooo" );
	SetChar( 3, "oooo:", "o:::o", "o:::o", "o:::o", "oooo:" );
	SetChar( 4, "ooooo", "o::::", "oooo:", "o::::", "ooooo" );
	SetChar( 5, "ooooo", "o::::", "ooo::", "o::::", "o::::" );
	SetChar( 6, ":oooo", "o::::", "o:ooo", "o:::o", ":ooo:" );
	SetChar( 7, "o:::o", "o:::o", "ooooo", "o:::o", "o:::o" );
	SetChar( 8, "::o::", "::o::", "::o::", "::o::", "::o::" );
	SetChar( 9, ":::o:", ":::o:", ":::o:", ":::o:", "ooo::" );
	SetChar( 10, "o::o:", "o:o::", "oo:::", "o:o::", "o::o:" );
	SetChar( 11, "o::::", "o::::", "o::::", "o::::", "ooooo" );
	SetChar( 12, "oo:o:", "o:o:o", "o:o:o", "o:::o", "o:::o" );
	SetChar( 13, "o:::o", "oo::o", "o:o:o", "o::oo", "o:::o" );
	SetChar( 14, ":ooo:", "o:::o", "o:::o", "o:::o", ":ooo:" );
	SetChar( 15, "oooo:", "o:::o", "oooo:", "o::::", "o::::" );
	SetChar( 16, ":ooo:", "o:::o", "o:::o", "o::oo", ":oooo" );
	SetChar( 17, "oooo:", "o:::o", "oooo:", "o:o::", "o::o:" );
	SetChar( 18, ":oooo", "o::::", ":ooo:", "::::o", "oooo:" );
	SetChar( 19, "ooooo", "::o::", "::o::", "::o::", "::o::" );
	SetChar( 20, "o:::o", "o:::o", "o:::o", "o:::o", ":oooo" );
	SetChar( 21, "o:::o", "o:::o", ":o:o:", ":o:o:", "::o::" );
	SetChar( 22, "o:::o", "o:::o", "o:o:o", "o:o:o", ":o:o:" );
	SetChar( 23, "o:::o", ":o:o:", "::o::", ":o:o:", "o:::o" );
	SetChar( 24, "o:::o", "o:::o", ":oooo", "::::o", ":ooo:" );
	SetChar( 25, "ooooo", ":::o:", "::o::", ":o:::", "ooooo" );
	SetChar( 26, ":ooo:", "o::oo", "o:o:o", "oo::o", ":ooo:" );
	SetChar( 27, "::o::", ":oo::", "::o::", "::o::", ":ooo:" );
	SetChar( 28, ":ooo:", "o:::o", "::oo:", ":o:::", "ooooo" );
	SetChar( 29, "oooo:", "::::o", "::oo:", "::::o", "oooo:" );
	SetChar( 30, "o::::", "o::o:", "ooooo", ":::o:", ":::o:" );
	SetChar( 31, "ooooo", "o::::", "oooo:", "::::o", "oooo:" );
	SetChar( 32, ":oooo", "o::::", "oooo:", "o:::o", ":ooo:" );
	SetChar( 33, "ooooo", "::::o", ":::o:", "::o::", "::o::" );
	SetChar( 34, ":ooo:", "o:::o", ":ooo:", "o:::o", ":ooo:" );
	SetChar( 35, ":ooo:", "o:::o", ":oooo", "::::o", ":ooo:" );
	SetChar( 36, "::o::", "::o::", "::o::", ":::::", "::o::" );
	SetChar( 37, ":ooo:", "::::o", ":::o:", ":::::", "::o::" );
	SetChar( 38, ":::::", ":::::", "::o::", ":::::", "::o::" );
	SetChar( 39, ":::::", ":::::", ":ooo:", ":::::", ":ooo:" );
	SetChar( 40, ":::::", ":::::", ":::::", ":::o:", "::o::" );
	SetChar( 41, ":::::", ":::::", ":::::", ":::::", "::o::" );
	SetChar( 42, ":::::", ":::::", ":ooo:", ":::::", ":::::" );
	SetChar( 43, ":::o:", "::o::", "::o::", "::o::", ":::o:" );
	SetChar( 44, "::o::", ":::o:", ":::o:", ":::o:", "::o::" );
	SetChar( 45, ":::::", ":::::", ":::::", ":::::", ":::::" );
	SetChar( 46, "ooooo", "ooooo", "ooooo", "ooooo", "ooooo" );
	SetChar( 47, "::o::", "::o::", ":::::", ":::::", ":::::" ); // Tnx Ferry
	SetChar( 48, "o:o:o", ":ooo:", "ooooo", ":ooo:", "o:o:o" );
	SetChar( 49, "::::o", ":::o:", "::o::", ":o:::", "o::::" );
	char c[] = "abcdefghijklmnopqrstuvwxyz0123456789!?:=,.-() #'*/";
	int i;
	for (i = 0; i < 256; i++) s_Transl[i] = 45;
	for (i = 0; i < 50; i++) s_Transl[(unsigned char)c[i]] = i;
}