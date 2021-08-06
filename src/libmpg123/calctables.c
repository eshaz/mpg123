/*
	calctables: compute fixed decoder table values

	copyright ?-2021 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp (as tabinit.c, and parts of layer2.c and layer3.c),
	then printout added by Thomas Orgis

	This is supposed to compute the supposedly fixed tables that used to be computed
	live on library startup in mpg123_init().
*/

#ifdef REAL_IS_FLOAT
#undef REAL_IS_FLOAT
#endif

#ifdef REAL_IS_FIXED
#undef REAL_IS_FIXED
#endif

#define REAL_IS_DOUBLE

#define RUNTIME_TABLES
#include "mpg123lib_intern.h"
#include "debug.h"
#include "l3tabs.h"
#include "l12tabs.h"
#include "costabs.h"

#define ASIZE(a) (sizeof(a)/sizeof(*a))

// helpers

static void print_char_array( const char *indent, const char *name
,	size_t count, char tab[] )
{
	size_t block = 72/4;
	size_t i = 0;
	if(name)
		printf("static const unsigned char %s[%zu] = \n", name, count);
	printf("%s{\n", indent);
	while(i<count)
	{
		size_t line = block > count-i ? count-i : block;
		printf("%s", indent);
		for(size_t j=0; j<line; ++j, ++i)
			printf("%s%c%3u", i ? "," : "", j ? ' ' : '\t', tab[i]);
		printf("\n");
	}
	printf("%s}%s\n", indent, name ? ";" : "");
}

static void print_value( int fixed, double fixed_scale
,	const char *name, double val )
{
	if(name)
		printf("static const real %s = ", name);
	if(fixed)
		printf("%ld;\n", (long)(double_to_long_rounded(fixed_scale*val, REAL_FACTOR)));
	else
		printf("%15.8e;\n", val);
}

// I feal uneasy about inf appearing as literal.
// Do all C99 implementations support it the same?
// An unreasonably big value should also just work.
static double limit_val(double val)
{
	if(val > 1e38)
		return 1e38;
	if(val < -1e38)
		return -1e38;
	return val;
}

static void print_array( int statick, int konst, int fixed, double fixed_scale
,	const char *indent, const char *name
,	size_t count, double tab[] )
{
	size_t block = 72/17;
	size_t i = 0;
	if(name)
		printf( "%s%s%sreal %s[%zu] = \n", statick ? "static " : ""
		,   konst ? "const " : ""
		,	fixed ? "" : "ALIGNED(16) ", name, count );
	printf("%s{\n", indent);
	while(i<count)
	{
		size_t line = block > count-i ? count-i : block;
		printf("%s", indent);
		if(fixed) for(size_t j=0; j<line; ++j, ++i)
			printf( "%s%c%11ld", i ? "," : "", j ? ' ' : '\t'
			,	(long)(double_to_long_rounded(fixed_scale*tab[i], REAL_FACTOR)) );
		else for(size_t j=0; j<line; ++j, ++i)
			printf("%s%c%15.8e", i ? "," : "", j ? ' ' : '\t', limit_val(tab[i]));
		printf("\n");
	}
	printf("%s}%s\n", indent, name ? ";" : "");
}

// C99 allows passing VLA with the fast dimensions first.
static void print_array2d( int fixed, double fixed_scale
,	const char *name, size_t x, size_t y
, double tab[][y] )
{
	printf( "static const%s real %s[%zu][%zu] = \n{\n", fixed ? "" : " ALIGNED(16)"
	,	name, x, y );
	for(size_t i=0; i<x; ++i)
	{
		if(i)
			printf(",");
		print_array(1, 1, fixed, fixed_scale, "\t", NULL, y, tab[i]);
	}
	printf("};\n");
}

static void print_short_array( const char *indent, const char *name
,	size_t count, short tab[] )
{
	size_t block = 72/8;
	size_t i = 0;
	if(name)
		printf("static const short %s[%zu] = \n", name, count);
	printf("%s{\n", indent);
	while(i<count)
	{
		size_t line = block > count-i ? count-i : block;
		printf("%s", indent);
		for(size_t j=0; j<line; ++j, ++i)
			printf("%s%c%6d", i ? "," : "", j ? ' ' : '\t', tab[i]);
		printf("\n");
	}
	printf("%s}%s\n", indent, name ? ";" : "");
}

static void print_fixed_array( const char *indent, const char *name
,	size_t count, real tab[] )
{
	size_t block = 72/13;
	size_t i = 0;
	if(name)
		printf("static const real %s[%zu] = \n", name, count);
	printf("%s{\n", indent);
	while(i<count)
	{
		size_t line = block > count-i ? count-i : block;
		printf("%s", indent);
		for(size_t j=0; j<line; ++j, ++i)
			printf("%s%c%11ld", i ? "," : "", j ? ' ' : '\t', (long)tab[i]);
		printf("\n");
	}
	printf("%s}%s\n", indent, name ? ";" : "");
}

static void print_ushort_array( const char *indent, const char *name
,	size_t count, unsigned short tab[] )
{
	size_t block = 72/8;
	size_t i = 0;
	if(name)
		printf("static const unsigned short %s[%zu] =\n", name, count);
	printf("%s{\n", indent);
	while(i<count)
	{
		size_t line = block > count-i ? count-i : block;
		printf("%s", indent);
		for(size_t j=0; j<line; ++j, ++i)
			printf("%s%c%8u", i ? "," : "", j ? ' ' : '\t', tab[i]);
		printf("\n");
	}
	printf("%s}%s\n", indent, name ? ";" : "");
}

// C99 allows passing VLA with the fast dimensions first.
static void print_short_array2d( const char *name, size_t x, size_t y
, short tab[][y] )
{
	printf( "static const short %s[%zu][%zu] =\n{\n"
	,	name, x, y );
	for(size_t i=0; i<x; ++i)
	{
		if(i)
			printf(",");
		print_short_array("\t", NULL, y, tab[i]);
	}
	printf("};\n");
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "usage:\n\t%s <cos|l12|l3>\n\n", argv[0]);
		return 1;
	}
	printf("// output of:\n// %s", argv[0]);
	for(int i=1; i<argc; ++i)
		printf(" %s", argv[i]);
	printf("\n\n");

	compute_costabs();
	compute_layer12();
	compute_layer3();

	for(int fixed=0; fixed < 2; ++fixed)
	{
		printf("\n#ifdef %s\n\n", fixed ? "REAL_IS_FIXED" : "REAL_IS_FLOAT");
		if(!fixed)
			printf("// aligned to 16 bytes for vector instructions, e.g. AltiVec\n\n");
		if(!strcmp("cos", argv[1]))
		{
			print_array(1, 0, fixed, 1., "", "cos64", ASIZE(cos64), cos64);
			print_array(1, 0, fixed, 1., "", "cos32", ASIZE(cos32), cos32);
			print_array(1, 0, fixed, 1., "", "cos16", ASIZE(cos16), cos16);
			print_array(1, 0, fixed, 1., "", "cos8",  ASIZE(cos8),  cos8 );
			print_array(1, 0, fixed, 1., "", "cos4",  ASIZE(cos4),  cos4 );
		}
		if(!strcmp("l12", argv[1]))
		{
			print_array2d(fixed, SCALE_LAYER12/REAL_FACTOR, "layer12_table", 27, 64, layer12_table);
		}
		if(!strcmp("l3", argv[1]))
		{
			print_array(1, 1, fixed, SCALE_POW43/REAL_FACTOR, "", "ispow"
			,	sizeof(ispow)/sizeof(*ispow), ispow );
			print_array(1, 1, fixed, 1., "", "aa_ca", ASIZE(aa_ca), aa_ca);
			print_array(1, 1, fixed, 1., "", "aa_cs", ASIZE(aa_cs), aa_cs);
			print_array2d(fixed, 1., "win", 4, 36, win);
			print_array2d(fixed, 1., "win1", 4, 36, win1);
			print_array(0, 1, fixed, 1., "", "COS9", ASIZE(COS9), COS9);
			print_value(fixed, 1., "COS6_1", COS6_1);
			print_value(fixed, 1., "COS6_2", COS6_2);
			print_array(0, 1, fixed, 1., "", "tfcos36", ASIZE(tfcos36), tfcos36);
			print_array(1, 1, fixed, 1., "", "tfcos12", ASIZE(tfcos12), tfcos12);
			print_array(1, 1, fixed, 1., "", "cos9", ASIZE(cos9), cos9);
			print_array(1, 1, fixed, 1., "", "cos18", ASIZE(cos18), cos18);
			print_array( 1, 1, fixed, SCALE_15/REAL_FACTOR, ""
			,	"tan1_1", ASIZE(tan1_1), tan1_1 );
			print_array( 1, 1, fixed, SCALE_15/REAL_FACTOR, ""
			,	"tan2_1", ASIZE(tan2_1), tan2_1 );
			print_array( 1, 1, fixed, SCALE_15/REAL_FACTOR, ""
			,	"tan1_2", ASIZE(tan1_2), tan1_2 );
			print_array( 1, 1, fixed, SCALE_15/REAL_FACTOR, ""
			,	"tan2_2", ASIZE(tan2_2), tan2_2 );
			print_array2d( fixed, SCALE_15/REAL_FACTOR
			,	"pow1_1", 2, 32, pow1_1 );
			print_array2d( fixed, SCALE_15/REAL_FACTOR
			,	"pow2_1", 2, 32, pow2_1 );
			print_array2d( fixed, SCALE_15/REAL_FACTOR
			,	"pow1_2", 2, 32, pow1_2 );
			print_array2d( fixed, SCALE_15/REAL_FACTOR
			,	"pow2_2", 2, 32, pow2_2 );
		}
		if(fixed)
			print_fixed_array("", "gainpow2", ASIZE(gainpow2), gainpow2);
		printf("\n#endif\n");
	}
	if(!strcmp("l12", argv[1]))
	{
		printf("\n");
		print_char_array("", "grp_3tab", ASIZE(grp_3tab), grp_3tab);
		printf("\n");
		print_char_array("", "grp_5tab", ASIZE(grp_5tab), grp_5tab);
		printf("\n");
		print_char_array("", "grp_9tab", ASIZE(grp_9tab), grp_9tab);
	}
	if(!strcmp("l3", argv[1]))
	{
		printf("\n");
		print_short_array2d("mapbuf0", 9, 152, mapbuf0);
		print_short_array2d("mapbuf1", 9, 156, mapbuf1);
		print_short_array2d("mapbuf2", 9,  44, mapbuf2);
		printf("static const short *map[9][3] =\n{\n");
		for(int i=0; i<9; ++i)
			printf( "%s\t{ mapbuf0[%d], mapbuf1[%d], mapbuf2[%d] }\n"
			,	(i ? "," : ""), i, i, i );
		printf("};\n");
		printf("static const short *mapend[9][3] =\n{\n");
		for(int i=0; i<9; ++i)
			printf( "%s\t{ mapbuf0[%d]+%d, mapbuf1[%d]+%d, mapbuf2[%d]+%d }\n"
			,	(i ? "," : "")
			,	i, (int)(mapend[i][0]-map[i][0])
			,	i, (int)(mapend[i][1]-map[i][1])
			,	i, (int)(mapend[i][2]-map[i][2]) );
		printf("};\n");
		print_ushort_array("", "n_slen2", ASIZE(n_slen2), n_slen2);
		print_ushort_array("", "i_slen2", ASIZE(i_slen2), i_slen2);
	}

	return 0;
}
