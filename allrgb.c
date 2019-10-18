// allrgb.c
// (c)2019 Bram Stolk

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "sino.h"
#include "hsv.h"
#include "write_pgm.h"

#include <stdlib.h>

#if 1
#	define SZ 4096
#else
#	define SZ 512
#endif

static float hue[SZ][SZ];
static float sat[SZ][SZ];
static float val[SZ][SZ];


typedef struct
{
	int x;
	int y;
	unsigned int r;
	unsigned int g;
	unsigned int b;
	// R7 G7 B7 R6 G6 B6 R5 G5 B5 R4 G4 B4 R3 G3 B3 R2 G2 B2 R1 G1 B1 R0 G0 B0
} pixel_t;


// Create a Simplex-Noise field, but do a double domain-warping first.
// So x,y warped to xx,yy.
// xx,yy warped to xxx,yyy.
// Then xxx,yyy used as index into simplex noise field.
// The noise has 4 octaves.
static void generate_field
(
	float *field,
	float OFF0,
	float OFF1,
	float OFF2,
	float OFF3,
	float OFF4,
	float OFF5,
	float OFF6,
	float OFF7
)
{
	const float f0 = 1.0f / SZ * 2;
	float *writer = field;
	for ( int y=0; y<SZ; ++y )
		for ( int x=0; x<SZ; ++x )
		{
			const float ox  = sino_2d_4o(  OFF0  - y * f0,  OFF1   + x * f0 );
			const float oy  = sino_2d_4o(  OFF2  + y * f0,  OFF3   + x * f0 );
			const float xx  = x + 40 * ox;
			const float yy  = y + 40 * oy;
			const float oxx = sino_2d_4o(  OFF4  - xx * f0,  OFF5 + yy * f0 );
			const float oyy = sino_2d_4o(  OFF6  + xx * f0,  OFF7 - yy * f0 );
			const float xxx = xx + 40 * oxx;
			const float yyy = yy + 40 * oyy;
			const float v  = sino_2d_4o( xxx * f0, yyy * f0 );
			*writer++ =  (1 + v)/2;	// remaps -1..1 to 0..1
		}
}


// We normalize the field, so that all the values between 0 and 1 are used.
static void normalize_field( float* field )
{
	float lo=1.0f;
	float hi=0.0f;
	for ( int i=0; i<SZ*SZ; ++i )
	{
		const float v = field[i];
		lo = v < lo ? v : lo;
		hi = v > hi ? v : hi;
	}
	fprintf( stderr, "range: %f..%f\n", lo, hi );
	float rng = hi-lo;
	const float scl = 1.0f / rng;
	for ( int i=0; i<SZ*SZ; ++i )
		field[i] = ( field[i] - lo ) * scl;
}


// Using hue, sat, val fields, create an RGB image.
static float* generate_image( const float* hue, const float* sat, const float *val )
{
	float* img = (float*)malloc( SZ * SZ * 3 * sizeof(float) );
	float* writer = img;
	int reader=0;
	for ( int y=0; y<SZ; ++y )
		for ( int x=0; x<SZ; ++x )
		{
			const float h = hue[reader];
			const float s = sat[reader];
			const float v = val[reader];
			reader++;
			hsv_to_rgb( h, s, v, writer+0, writer+1, writer+2 );
			writer += 3;
		}
	return img;
}


// Create a HDR image with 32-bit R, 32-bit G and 32-bit B channels.
// Record the x,y position of each pixel, so we can look that up after sorting.
static void generate_hdr( pixel_t* hdr, const float* img )
{
	const float* reader = img;
	pixel_t* writer = hdr;

	for ( int y=0; y<SZ; ++y )
		for ( int x=0; x<SZ; ++x )
		{
			writer->x = x;
			writer->y = y;
			writer->r = (unsigned int) ( 0xffffffff * reader[0] );
			writer->g = (unsigned int) ( 0xffffffff * reader[1] );
			writer->b = (unsigned int) ( 0xffffffff * reader[2] );
			reader+=3;
			writer++;
		}
}


// Used by qsort() to sort pixels on RGB values.
static int compare_pixels( const void* a, const void * b )
{
	const pixel_t* p0 = (const pixel_t*) a;
	const pixel_t* p1 = (const pixel_t*) b;
	for (int bit=31; bit>=0; bit--)
	{
		unsigned int msk = 0x1 << bit;
		unsigned int r0 = (p0->r) & msk;
		unsigned int r1 = (p1->r) & msk;
		unsigned int g0 = (p0->g) & msk;
		unsigned int g1 = (p1->g) & msk;
		unsigned int b0 = (p0->b) & msk;
		unsigned int b1 = (p1->b) & msk;
		if ( r0 > r1 ) return  1;
		if ( r0 < r1 ) return -1;
		if ( g0 > g1 ) return  1;
		if ( g0 < g1 ) return -1;
		if ( b0 > b1 ) return  1;
		if ( b0 < b1 ) return -1;
	}
	return 0;
}


// Convert an index into the sorted list (0x000000 to 0xffffff) back into an 24b RGB colour.
static void get_colour(int i, unsigned char* rgb)
{
	// R7 G7 B7 R6 G6 B6 R5 G5 B5 R4 G4 B4 R3 G3 B3 R2 G2 B2 R1 G1 B1 R0 G0 B0
	rgb[0] =
		( ((i>>23)&1) << 7 ) |
		( ((i>>20)&1) << 6 ) |
		( ((i>>17)&1) << 5 ) |
		( ((i>>14)&1) << 4 ) |
		( ((i>>11)&1) << 3 ) |
		( ((i>> 8)&1) << 2 ) |
		( ((i>> 5)&1) << 1 ) |
		( ((i>> 2)&1) << 0 );
	rgb[1] =
		( ((i>>22)&1) << 7 ) |
		( ((i>>19)&1) << 6 ) |
		( ((i>>16)&1) << 5 ) |
		( ((i>>13)&1) << 4 ) |
		( ((i>>10)&1) << 3 ) |
		( ((i>> 7)&1) << 2 ) |
		( ((i>> 4)&1) << 1 ) |
		( ((i>> 1)&1) << 0 );
	rgb[2] =
		( ((i>>21)&1) << 7 ) |
		( ((i>>18)&1) << 6 ) |
		( ((i>>15)&1) << 5 ) |
		( ((i>>12)&1) << 4 ) |
		( ((i>> 9)&1) << 3 ) |
		( ((i>> 6)&1) << 2 ) |
		( ((i>> 3)&1) << 1 ) |
		( ((i>> 0)&1) << 0 );
}


// Main program that uses Simplex Noise to generate HSV fields, converts to RGB, sorts on RGB, and generates the output with a unique colour for each pixel.
int main( int argc, char* argv[] )
{
	sino_init();	// Initialize SimplexNoise data tables.

	generate_field( (float*)hue,  0.45f, -0.57f, 0.123f, -4.8f, -2.2f,  0.33f, -0.22f, 0.12f );
	generate_field( (float*)val, -0.55f,  0.22f, 0.955f, -1.5f,  0.5f, -0.99f,  2.48f, 2.09f );
	generate_field( (float*)sat, -3.33f,  2.29f,-0.111f,  2.2f,  0.8f, -0.22f,  1.11f, 1.02f );

	normalize_field( (float*)hue );
	normalize_field( (float*)val );
	normalize_field( (float*)sat );

	const float* img = generate_image( (float*)hue, (float*)sat, (float*)val );

	pixel_t* hdr = (pixel_t*) malloc( SZ * SZ * sizeof(pixel_t) );
	generate_hdr( hdr, img );

	unsigned char* out = (unsigned char*) malloc( SZ*SZ*3 );

	qsort( hdr, SZ*SZ, sizeof(pixel_t), compare_pixels );

	for ( int i=0; i<SZ*SZ; ++i )
	{
		const int x = hdr[i].x;
		const int y = hdr[i].y;
		unsigned char* writer = out + ( y * SZ + x ) * 3;
		get_colour( i, writer );
	}

	// Somehow, this doesn't work for 4096x4096 images?
	//const int stride = SZ*3;
	//stbi_write_png( "out.png", SZ, SZ, 3, out, stride );

	FILE* f;

	f = fopen("all.ppm", "wb" );
	write_ppm_3channel_int( f, out, SZ );
	fclose(f);

	f = fopen("hue.ppm", "wb");
	write_ppm( f, (float*) hue, SZ );
	fclose(f);

	f = fopen("sat.ppm", "wb");
	write_ppm( f, (float*) sat, SZ );
	fclose(f);

	f = fopen("val.ppm", "wb");
	write_ppm( f, (float*) val, SZ );
	fclose(f);

	f = fopen("out.ppm", "wb");
	write_ppm_3channel( f, (float*) img, SZ );
	fclose(f);

	sino_exit();	// Clean Up SimplexNoise data tables.

	return 0;
}

