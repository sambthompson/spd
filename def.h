/*
 * def.h contains some useful definitions for "C" programs.
 *
 * Author:  Eric Haines, 3D/Eye, Inc.
 *
 * Modified: 1 October 1992
 *           Alexander R. Enzmann
 * Modified: 14 December 1992  - Better Mac (MPW) compatibility
 *           Eduard [esp] Schwan
 * Modified: 2 August 1993  - More ANSI C & Mac compatibility fixes
 *           Eduard [esp] Schwan
 * Modified: 4 September 1993  - Added one more generator (LATTICE)
 *           Antonio [acc] Costa
 * Modified: 6 September 1993  - Changed LATTICE to GENERIC, other renames
 *           Eric Haines
 * Modified: 16 September 1993  - Added SPD_LATTICE & SPD_SHELLS defines
 *           Eduard [esp] Schwan
 * Modified: 27 July 1994  - Added __MWERKS__ define for Metrowerks compiler
 *           Eduard [esp] Schwan
 * Modified: 2 November 1994  - Added COMB_COORD
 *           Alexander R. Enzmann
 *
 */

#ifndef DEF_H
#define DEF_H

#if __cplusplus
extern "C" {
#endif

/*
 * Each data base/program is defined here.  These are used as
 * parameters to the PLATFORM_INIT() macro, in case the programs
 * need to behave differently based on the type of db run.  For
 * example, the Macintosh version must prompt the user for input
 * parameters instead of getting them on the command line.  This
 * allows the Mac to use a different dialog box for each program.
 */
#define SPD_MIN                  1
#define SPD_BALLS              1
#define SPD_GEARS              2
#define SPD_MOUNT              3
#define SPD_RINGS              4
#define SPD_TEAPOT             5
#define SPD_TETRA              6
#define SPD_TREE               7
#define SPD_READDXF            8
#define SPD_LATTICE            9
#define SPD_SHELLS             10
#define SPD_READNFF           11
#define SPD_READOBJ           12
#define SPD_GENERIC           13
#define SPD_MAX                       SPD_GENERIC


/* ---- Macintosh-specific definitions here ---- */

#if defined(applec) || defined(THINK_C) || defined(__MWERKS__)
#define PARAMS(x)  x

/* If OUTPUT_TO_FILE is defined, output goes to new text file, not to stdout */
#define OUTPUT_TO_FILE                   true
#define PLATFORM_INIT(spdType)      MacInit(&argc, &argv, spdType)
#define PLATFORM_MULTITASK()      MacMultiTask()
#define PLATFORM_SHUTDOWN()               MacShutDown()

#define EPSILON  1.0e-15
#define EPSILON2 1.0e-7


#if defined(applec)
#include <sane.h>
#define PI      pi()
#endif /* applec */


/*
 * declared in MAC.C
 */
extern void             MacInit(int *argcp, char ***argvp, int spdType);
extern void             MacMultiTask(void);
extern void          MacShutDown(void);
/* extern short       macParmOutFormat; */
/* extern short     macParmSize; */
/* extern short  macParm2; */
#endif /* applec || THINK_C */



/* Use "PARAMS(x) ()" above if your compiler can't handle function prototyping,
 * "PARAMS(x) x" if it can use ANSI headers.
 */
#ifndef PARAMS

/* check if ANSI headers should be available */
#if __STDC__ || defined(__cplusplus)
/* yes, ANSI headers */
#define PARAMS(x)  x
#else
/* no, no ANSI headers */
#define PARAMS(x) ()
#endif

#endif


/*
 * PLATFORM_xxx macros are used to allow different OS platforms
 * hooks to do their own initialization, periodic tasks, and
 * shutdown cleanup, if needed. [esp]
 */

#ifndef PLATFORM_INIT
#define PLATFORM_INIT(spdType)
#endif /* PLATFORM_INIT */

#ifndef PLATFORM_MULTITASK
#define PLATFORM_MULTITASK()
#endif /* PLATFORM_MULTITASK */

#ifndef PLATFORM_SHUTDOWN
#define PLATFORM_SHUTDOWN()
#endif /* PLATFORM_SHUTDOWN */


/* exit codes - define as you wish */
#define EXIT_SUCCESS    0
#define        EXIT_FAIL       1

#ifndef EPSILON
#define EPSILON 1.0e-8
#endif

#ifndef EPSILON2
#define EPSILON2 1.0e-6
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef PI
#define PI 3.141592653589793
#endif

typedef double MATRIX[4][4] ;  /* row major form */

typedef       double  COORD3[3] ;
typedef      double  COORD4[4] ;

/* COORD3/COORD4 indeces */
#define   X       0
#define        Y       1
#define        Z       2
#define        W       3

/* COORD3 (color) indeces */
#define    R_COLOR 0
#define        G_COLOR 1
#define        B_COLOR 2

#define POW(A,B)     ( (A) == 0.0 ? 0.0 : ( (B) == 0.0 ? 1.0 : pow(A, B) ) )
#define SGN(A)       ( (A) < 0.0 ? -1.0 : ( (A) > 0.0 ? 1.0 : 0.0) )
#define ABSOLUTE(A)            ( (A) < 0 ? -(A) : (A) )
#define FRACTION(A)             ( (A) - (int)(A) )
#define       IS_VAL_ALMOST_ZERO(A,E) ( ABSOLUTE(A) <= (E) )
#define   MAX(A,B)                ( (A) > (B) ? (A) : (B) )
#define        MIN(A,B)                ( (A) < (B) ? (A) : (B) )
#define SQR(A)                 ( (A) * (A) )

#define ADD2_COORD3(r,a)   { (r)[X] += (a)[X]; (r)[Y] += (a)[Y];\
			    (r)[Z] += (a)[Z]; }
#define ADD3_COORD3(r,a,b) { (r)[X] = (a)[X] + (b)[X];\
			      (r)[Y] = (a)[Y] + (b)[Y];\
			      (r)[Z] = (a)[Z] + (b)[Z]; }
#define COPY_COORD3(r,a)   { (r)[X] = (a)[X];\
			       (r)[Y] = (a)[Y];\
			       (r)[Z] = (a)[Z];}
#define COPY_COORD4(r,a)     { (r)[X] = (a)[X];\
			       (r)[Y] = (a)[Y];\
			       (r)[Z] = (a)[Z];\
			       (r)[W] = (a)[W]; }
#define CROSS(r,a,b)                { (r)[X] = (a)[Y] * (b)[Z] - (a)[Z] * (b)[Y];\
			    (r)[Y] = (a)[Z] * (b)[X] - (a)[X] * (b)[Z];\
			    (r)[Z] = (a)[X] * (b)[Y] - (a)[Y] * (b)[X]; }
#define DOT_PRODUCT(a,b) ( (a)[X] * (b)[X] +\
			      (a)[Y] * (b)[Y] +\
			      (a)[Z] * (b)[Z] )
#define DOT4(a,b)            ( (a)[X] * (b)[X] +\
			      (a)[Y] * (b)[Y] +\
			      (a)[Z] * (b)[Z] +\
			      (a)[W] * (b)[W] )
#define IS_COORD3_ALMOST_ZERO(a,E)   (\
				   IS_VAL_ALMOST_ZERO( (a)[X], (E) )\
			   && IS_VAL_ALMOST_ZERO( (a)[Y], (E) )\
			   && IS_VAL_ALMOST_ZERO( (a)[Z], (E) ) )
#define SET_COORD3(r,A,B,C)     { (r)[X] = (A); (r)[Y] = (B); (r)[Z] = (C); }
#define SET_COORD4(r,A,B,C,D)      { (r)[X] = (A); (r)[Y] = (B); (r)[Z] = (C);\
			      (r)[W] = (D); }
#define SUB2_COORD3(r,a)       { (r)[X] -= (a)[X]; (r)[Y] -= (a)[Y];\
			    (r)[Z] -= (a)[Z]; }
#define SUB3_COORD3(r,a,b) { (r)[X] = (a)[X] - (b)[X];\
			      (r)[Y] = (a)[Y] - (b)[Y];\
			      (r)[Z] = (a)[Z] - (b)[Z]; }
#define LERP_COORD(r, a, b, u) { (r)[X] = (a)[X] + (u) * ((b)[X] - (a)[X]);\
				(r)[Y] = (a)[Y] + (u) * ((b)[Y] - (a)[Y]);\
			     (r)[Z] = (a)[Z] + (u) * ((b)[Z] - (a)[Z]); }
#define COMB_COORD(r, a, b, u, v) { (r)[X] = (a)[X] * (u) + (b)[X] * (v);\
				    (r)[Y] = (a)[Y] * (u) + (b)[X] * (v);\
				    (r)[Z] = (a)[Z] * (u) + (b)[X] * (v); }
#define RAD2DEG(x) (180.0*(x)/3.1415926535897932384626)

#if __cplusplus
}
#endif


#endif /* DEF_H */
