// Template, IGAD version 3, Raspberry PI 4 version
// Get the latest version from: https://github.com/jbikker/tmpl8pi
// IGAD/NHTV/BUAS/UU - Jacco Bikker - 2006-2023

// default screen resolution
#define SCRWIDTH	640
#define SCRHEIGHT	480

// allow NEON code
#define USE_NEON_SIMD	1

// constants
#define PI			3.14159265358979323846264f
#define INVPI		0.31830988618379067153777f
#define INV2PI		0.15915494309189533576888f
#define TWOPI		6.28318530717958647692528f
#define SQRT_PI_INV	0.56418958355f
#define LARGE_FLOAT	1e34f

// detect windows build
#ifdef _MSC_VER
#define WINBUILD	1
#else
#define WINBUILD	0
#endif

// basic types
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
#if WINBUILD == 0
typedef long LONG;
#endif

// generic includes
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h> 
#include <chrono>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <EGL/eglext.h> // EGL_OPENGL_ES3_BIT_KHR etc.
#include <X11/Xlib.h> // Display object etc.

#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <fstream>
#include <pthread.h>

#include <linux/input.h>
#include <linux/input-event-codes.h>

#ifdef USE_NEON_SIMD

#include <arm_neon.h>

inline float hormin( const float32x4_t v4 )
{
	const float32x2_t minOfHalfs = vpmin_f32( vget_low_f32( v4 ), vget_high_f32( v4 ) );
	const float32x2_t minOfMinOfHalfs = vpmin_f32( minOfHalfs, minOfHalfs );
	return vget_lane_f32( minOfMinOfHalfs, 0 );
}

inline float hormax( const float32x4_t v4 )
{
	const float32x2_t maxOfHalfs = vpmax_f32( vget_low_f32( v4 ), vget_high_f32( v4 ) );
	const float32x2_t maxOfMaxOfHalfs = vpmax_f32( maxOfHalfs, maxOfHalfs );
	return vget_lane_f32( maxOfMaxOfHalfs, 0 );
}

inline float32x4_t reciproc4( const float32x4_t v4 )
{
	float32x4_t reci4 = vrecpeq_f32( v4 ); // just an approximation
	reci4 = vmulq_f32( vrecpsq_f32( v4, reci4 ), reci4 ); // Newton-Raphson #1
	return vmulq_f32( vrecpsq_f32( v4, reci4 ), reci4 ); // Newton-Raphson #2
}

#endif

using namespace std;

// include the surface and sprite classes and related functionality
#include "surface.h"
#include "sprite.h"

using namespace Tmpl8;

// aligned memory allocations
#if WINBUILD
#define ALIGN( x ) __declspec( align( x ) )
#define MALLOC64( x ) ( ( x ) == 0 ? 0 : _aligned_malloc( ( x ), 64 ) )
#define FREE64( x ) _aligned_free( x )
#else
#define ALIGN( x ) __attribute__( ( aligned( x ) ) )
#define MALLOC64( x ) ( ( x ) == 0 ? 0 : aligned_alloc( 64, ( x ) ) )
#define FREE64( x ) free( x )
#endif
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CHECK_RESULT __attribute__ ((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECK_RESULT _Check_return_
#else
#define CHECK_RESULT
#endif

// include math classes
#include "tmpl8math.h"

// fatal error reporting (with a pretty window)
void FatalError( const char* fmt, ... );
#define FATALERROR( fmt, ... ) FatalError( "Error on line %d of %s: " fmt "\n", __LINE__, __FILE__, ##__VA_ARGS__ )
#define FATALERROR_IF( condition, fmt, ... ) do { if ( ( condition ) ) FATALERROR( fmt, ##__VA_ARGS__ ); } while ( 0 )
#define FATALERROR_IN( prefix, errstr, fmt, ... ) FatalError( prefix " returned error '%s' at %s:%d" fmt "\n", errstr, __FILE__, __LINE__, ##__VA_ARGS__ );
#define FATALERROR_IN_CALL( stmt, error_parser, fmt, ... ) do { auto ret = ( stmt ); if ( ret ) FATALERROR_IN( #stmt, error_parser( ret ), fmt, ##__VA_ARGS__ ) } while ( 0 )

// include opengl template functionality
#include "opengl.h"

// generic error checking for OpenGL code
#define CheckGL() { _CheckGL( __FILE__, __LINE__ ); }

// forward declarations of helper functions
void _CheckGL( const char* f, int l );
GLuint CreateVBO( const GLfloat* data, const uint size );
void BindVBO( const uint idx, const uint N, const GLuint id );
void CheckShader( GLuint shader, const char* vshader, const char* fshader );
void CheckProgram( GLuint id, const char* vshader, const char* fshader );
void DrawQuad();
void FixWorkingFolder();
string TextFileRead( const char* _File );

// timer
struct Timer
{
	Timer() { reset(); }
	float elapsed() const
	{
		chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
		chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - start);
		return (float)time_span.count();
	}
	void reset() { start = chrono::high_resolution_clock::now(); }
	chrono::high_resolution_clock::time_point start;
};

#include "game.h"

// EOF