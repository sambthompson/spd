/*
 * libvec.c - a library of vector operations and a random number generator
 *
 * Author:  Eric Haines, 3D/Eye, Inc.
 *
 * Modified: 1 October 1992
 *	     Alexander R. Enzmann
 *	     Object generation routines split off to keep file size down
 *
 * Modified: 17 Jan 1993
 *	     Eduard [esp] Schwan
 *	     Removed unused local variables, added <stdlib.h> include for
 *	     exit() call
 */

#include <stdio.h>
#include <stdlib.h> /* exit() */
#include <math.h>
#include "libvec.h"

/*
 * Normalize the vector (X,Y,Z) so that X*X + Y*Y + Z*Z = 1.
 *
 * The normalization divisor is returned.  If the divisor is zero, no
 * normalization occurs.
 *
 */
double
lib_normalize_vector(cvec)
    COORD3 cvec;
{
    double divisor;

    divisor = sqrt( (double)DOT_PRODUCT(cvec, cvec) );
    if (divisor > 0.0) {
	cvec[X] /= divisor;
	cvec[Y] /= divisor;
	cvec[Z] /= divisor;
    }
    return divisor;
}


/*
 * Set all matrix elements to zero.
 */
void
lib_zero_matrix(mx)
    MATRIX mx;
{
    int i, j;

    for (i=0;i<4;++i)
	for (j=0;j<4;++j)
	    mx[i][j] = 0.0;
}

/*
 * Create identity matrix.
 */
void
lib_create_identity_matrix(mx)
    MATRIX mx;
{
    int i;

    lib_zero_matrix(mx);
    for (i=0;i<4;++i)
	mx[i][i] = 1.0;
}

/*
 * Copy one matrix to another
 */
void
lib_copy_matrix(mres, mx)
    MATRIX mres, mx;
{
    int i, j;

    for (i=0;i<4;i++)
	for (j=0;j<4;j++)
	    mres[i][j] = mx[i][j];
}

/*
 * Create a rotation matrix along the given axis by the given angle in radians.
 */
void
lib_create_rotate_matrix(mx, axis, angle)
    MATRIX mx;
    int axis;
    double angle;
{
    double cosine, sine;

    lib_zero_matrix(mx);
    cosine = cos((double)angle);
    sine = sin((double)angle);
    switch (axis) {
	case X_AXIS:
	    mx[0][0] = 1.0;
	    mx[1][1] = cosine;
	    mx[2][2] = cosine;
	    mx[1][2] = sine;
	    mx[2][1] = -sine;
	    break ;
	case Y_AXIS:
	    mx[1][1] = 1.0;
	    mx[0][0] = cosine;
	    mx[2][2] = cosine;
	    mx[2][0] = sine;
	    mx[0][2] = -sine;
	    break;
	case Z_AXIS:
	    mx[2][2] = 1.0;
	    mx[0][0] = cosine;
	    mx[1][1] = cosine;
	    mx[0][1] = sine;
	    mx[1][0] = -sine;
	    break;
    }
    mx[3][3] = 1.0;
}

/*
 * Create a rotation matrix along the given axis by the given angle in radians.
 * The axis is a set of direction cosines.
 */
void
lib_create_axis_rotate_matrix(mx, axis, angle)
    MATRIX mx;
    COORD3 axis;
    double angle;
{
    double  cosine, one_minus_cosine, sine;

    cosine = cos((double)angle);
    sine = sin((double)angle);
    one_minus_cosine = 1.0 - cosine;

    mx[0][0] = SQR(axis[X]) + (1.0 - SQR(axis[X])) * cosine;
    mx[0][1] = axis[X] * axis[Y] * one_minus_cosine + axis[Z] * sine;
    mx[0][2] = axis[X] * axis[Z] * one_minus_cosine - axis[Y] * sine;
    mx[0][3] = 0.0;

    mx[1][0] = axis[X] * axis[Y] * one_minus_cosine - axis[Z] * sine;
    mx[1][1] = SQR(axis[Y]) + (1.0 - SQR(axis[Y])) * cosine;
    mx[1][2] = axis[Y] * axis[Z] * one_minus_cosine + axis[X] * sine;
    mx[1][3] = 0.0;

    mx[2][0] = axis[X] * axis[Z] * one_minus_cosine + axis[Y] * sine;
    mx[2][1] = axis[Y] * axis[Z] * one_minus_cosine - axis[X] * sine;
    mx[2][2] = SQR(axis[Z]) + (1.0 - SQR(axis[Z])) * cosine;
    mx[2][3] = 0.0;

    mx[3][0] = 0.0;
    mx[3][1] = 0.0;
    mx[3][2] = 0.0;
    mx[3][3] = 1.0;
}

/* Create translation matrix */
void
lib_create_translate_matrix(mx, vec)
    MATRIX mx;
    COORD3 vec;
{
    lib_create_identity_matrix(mx);
    mx[3][0] = vec[X];
    mx[3][1] = vec[Y];
    mx[3][2] = vec[Z];
}

/* Create scaling matrix */
void
lib_create_scale_matrix(mx, vec)
    MATRIX mx;
    COORD3 vec;
{
    lib_zero_matrix(mx);
    mx[0][0] = vec[X];
    mx[1][1] = vec[Y];
    mx[2][2] = vec[Z];
}

/* Given a point and a direction, find the transform that brings a point
   in a canonical coordinate system into a coordinate system defined by
   that position and direction.    Both the forward and inverse transform
   matrices are calculated. */
void
lib_create_canonical_matrix(trans, itrans, origin, up)
    MATRIX trans, itrans;
    COORD3 origin;
    COORD3 up;
{
    MATRIX trans1, trans2;
    COORD3 tmpv;
    double ang;

    /* Translate "origin" to <0, 0, 0> */
    SET_COORD3(tmpv, -origin[X], -origin[Y], -origin[Z]);
    lib_create_translate_matrix(trans1, tmpv);

    /* Determine the axis to rotate about */
    if (fabs(up[Z]) == 1.0)
	SET_COORD3(tmpv, 1.0, 0.0, 0.0)
    else
	SET_COORD3(tmpv, -up[Y], up[X], 0.0)
    lib_normalize_vector(tmpv);
    ang = acos(up[Z]);

    /* Calculate the forward matrix */
    lib_create_axis_rotate_matrix(trans2, tmpv, -ang);
    lib_matrix_multiply(trans, trans1, trans2);

    /* Calculate the inverse transform */
    lib_create_axis_rotate_matrix(trans1, tmpv, ang);
    lib_create_translate_matrix(trans2, origin);
    lib_matrix_multiply(itrans, trans1, trans2);
}

/* Find the transformation that takes the current eye position and
   orientation and puts it at {0, 0, 0} with the up vector aligned
   with {0, 1, 0} */
void
lib_create_view_matrix(trans, from, at, up, xres, yres, angle, aspect)
    MATRIX trans;
    COORD3 from, at, up;
    int xres, yres;
    double angle, aspect;
{
    double x, y, z, d, theta;
    double xscale, yscale, view_dist;
    COORD3 Rt, right, up_dir;
    COORD3 Va, Ve;
    MATRIX Tv, Tt, Tx;

    /* Change the view angle into radians */
    angle = PI * angle / 180.0;

    /* Translate point of interest to the origin */
    SET_COORD3(Va, -at[X], -at[Y], -at[Z]);
    lib_create_translate_matrix(Tv, Va);

    /* Move the eye by the same amount */
    ADD3_COORD3(Ve, from, Va);

    /* Make sure this is a valid sort of setup */
    if (Ve[X] == 0.0 && Ve[Y] == 0.0 && Ve[Z] == 0.0) {
	fprintf(stderr, "Degenerate perspective transformation\n");
	exit(1);
    }

    /* Get the up vector to be perpendicular to the view vector */
    SET_COORD3(Rt, -Ve[X], -Ve[Y], -Ve[Z]);
    lib_normalize_vector(Rt);
    COPY_COORD3(up_dir, up);
    CROSS(right, up_dir, Rt);
    lib_normalize_vector(right);
    CROSS(up_dir, Rt, right);
    lib_normalize_vector(up_dir);

    /* Determine the angle of rotation about the x-axis that takes
      the eye into the x-z plane. */
    y = Ve[Y];
    if (fabs(y) > EPSILON) {
	z = Ve[Z];
	d = sqrt(y * y + z * z);
	if (fabs(d) > EPSILON) {
	    if (y > 0)
		theta = acos(z/d) - PI;
	    else
		theta = -acos(z/d) - PI;
	    lib_create_rotate_matrix(Tt, X_AXIS, theta);
	    lib_matrix_multiply(Tx, Tv, Tt);
	    lib_copy_matrix(Tv, Tx);
	}
    }

    /* Determine the angle of rotation about the y-axis that takes
	the eye onto the minus z-axis */
    lib_transform_point(Ve, from, Tv);
    x = Ve[X];
    z = Ve[Z];
    if (fabs(x) > EPSILON) {
	d = sqrt(x * x + z * z);
	if (fabs(d) > EPSILON) {
	    if (x > 0)
		theta = PI - acos(z / d);
	    else
		theta = PI + acos(z / d);
	    lib_create_rotate_matrix(Tt, Y_AXIS, theta);
	    lib_matrix_multiply(Tx, Tv, Tt);
	    lib_copy_matrix(Tv, Tx);
	}
    } else if (z > 0) {
	lib_create_rotate_matrix(Tt, Y_AXIS, PI);
	lib_matrix_multiply(Tx, Tv, Tt);
	lib_copy_matrix(Tv, Tx);
    }

    /* Determine the angle of rotation about the z-axis that takes
	the up vector into alignment with the y-axis */
    lib_transform_vector(Ve, up_dir, Tv);
    y = Ve[Y];
    if (y < 1.0) {
	x = Ve[X];
	d = sqrt(x * x + y * y);
	if (fabs(d) > EPSILON) {
	    if (x > 0)
		theta = acos(y / d);
	    else
		theta = -acos(y / d);
	    lib_create_rotate_matrix(Tt, Z_AXIS, theta);
	    lib_matrix_multiply(Tx, Tv, Tt);
	    lib_copy_matrix(Tv, Tx);
	}
    }

    /* Finally, translate the eye position to the point {0, 0, 0} */
    SUB3_COORD3(Ve, at, from);
    view_dist = sqrt(DOT_PRODUCT(Ve, Ve));
    lib_transform_point(Ve, from, Tv);
    SET_COORD3(Va, 0, 0, -Ve[Z]);
    lib_create_translate_matrix(Tt, Va);
    lib_matrix_multiply(Tx, Tv, Tt);
    lib_copy_matrix(Tv, Tx);

    /* Determine how much to scale things by */
    yscale = (double)yres / (2.0 * view_dist * tan(angle/2.0));
    xscale = yscale * (double)xres / ((double)yres * aspect);
    SET_COORD3(Va, xscale, yscale, 1.0);
    lib_create_scale_matrix(Tt, Va);
    lib_matrix_multiply(Tx, Tv, Tt);
    lib_copy_matrix(Tv, Tx);

    /* Now add in the perspective transformation */
    lib_create_identity_matrix(Tt);
    Tt[2][3] = 1.0 / view_dist;
    lib_matrix_multiply(Tx, Tv, Tt);

    /* We now have the final transformation matrix */
    lib_copy_matrix(trans, Tx);
}

/*
 * Multiply a vector by a matrix.
 */
void
lib_transform_vector(vres, vec, mx)
    COORD3 vres, vec;
    MATRIX mx;
{
    vres[X] = vec[X]*mx[0][0] + vec[Y]*mx[1][0] + vec[Z]*mx[2][0];
    vres[Y] = vec[X]*mx[0][1] + vec[Y]*mx[1][1] + vec[Z]*mx[2][1];
    vres[Z] = vec[X]*mx[0][2] + vec[Y]*mx[1][2] + vec[Z]*mx[2][2];
}

/*
 * Multiply a point by a matrix.
 */
void
lib_transform_point(vres, vec, mx)
    COORD3 vres, vec;
    MATRIX mx;
{
    vres[X] = vec[X]*mx[0][0] + vec[Y]*mx[1][0] + vec[Z]*mx[2][0] + mx[3][0];
    vres[Y] = vec[X]*mx[0][1] + vec[Y]*mx[1][1] + vec[Z]*mx[2][1] + mx[3][1];
    vres[Z] = vec[X]*mx[0][2] + vec[Y]*mx[1][2] + vec[Z]*mx[2][2] + mx[3][2];
}

/*
 * Multiply a 4 element vector by a matrix.
 */
void
lib_transform_coord(vres, vec, mx)
    COORD4 vres, vec;
    MATRIX mx;
{
    vres[X] = vec[X]*mx[0][0]+vec[Y]*mx[1][0]+vec[Z]*mx[2][0]+vec[W]*mx[3][0];
    vres[Y] = vec[X]*mx[0][1]+vec[Y]*mx[1][1]+vec[Z]*mx[2][1]+vec[W]*mx[3][1];
    vres[Z] = vec[X]*mx[0][2]+vec[Y]*mx[1][2]+vec[Z]*mx[2][2]+vec[W]*mx[3][2];
    vres[W] = vec[X]*mx[0][3]+vec[Y]*mx[1][3]+vec[Z]*mx[2][3]+vec[W]*mx[3][3];
}

/*
 * Compute transpose of matrix.
 */
void
lib_transpose_matrix( mxres, mx )
    MATRIX mxres, mx;
{
    int i, j;

    for (i=0;i<4;i++)
	for (j=0;j<4;j++)
	    mxres[j][i] = mx[i][j];
}

/*
 * Multiply two 4x4 matrices.
 */
void
lib_matrix_multiply(mxres, mx1, mx2)
    MATRIX mxres, mx1, mx2;
{
    int i, j;

    for (i=0;i<4;i++)
	for (j=0;j<4;j++)
	    mxres[i][j] = mx1[i][0]*mx2[0][j] + mx1[i][1]*mx2[1][j] +
			      mx1[i][2]*mx2[2][j] + mx1[i][3]*mx2[3][j];
}

/* Performs a 3D clip of a line segment from start to end against the
    box defined by bounds.  The actual values of start and end are modified. */
int
lib_clip_to_box(start, end,  bounds)
    COORD3 start, end;
    double bounds[2][3];
{
    int i;
    double d, t, dir, pos;
    double tmin, tmax;
    COORD3 D;

    SUB3_COORD3(D, end, start);
    d = lib_normalize_vector(D);
    tmin = 0.0;
    tmax = d;
    for (i=0;i<3;i++) {
	pos = start[i];
	dir = D[i];
	if (dir < -EPSILON) {
	    t = (bounds[0][i] - pos) / dir;
	    if (t < tmin)
		return 0;
	    if (t <= tmax)
		tmax = t;
	    t = (bounds[1][i] - pos) / dir;
	    if (t >= tmin) {
		if (t > tmax)
		    return 0;
		tmin = t;
	    }
	} else if (dir > EPSILON) {
	    t = (bounds[1][i] - pos) / dir;
	    if (t < tmin)
		return 0;
	    if (t <= tmax)
		tmax = t;
	    t = (bounds[0][i] - pos) / dir;
	    if (t >= tmin) {
		if (t > tmax)
		    return 0;
		tmin = t;
	    }
	}
	else if (pos < bounds[0][i] || pos > bounds[1][i])
	    return 0;
    }
    if (tmax < d) {
	end[X] = start[X] + tmax * D[X];
	end[Y] = start[Y] + tmax * D[Y];
	end[Z] = start[Z] + tmax * D[Z];
    }
    if (tmin > 0.0) {
	start[X] = start[X] + tmin * D[X];
	start[Y] = start[Y] + tmin * D[Y];
	start[Z] = start[Z] + tmin * D[Z];
    }
    return 1;
}

/*
 * Rotate a vector pointing towards the major-axis faces (i.e. the major-axis
 * component of the vector is defined as the largest value) 90 degrees to
 * another cube face.  Mod_face is a face number.
 *
 * If the routine is called six times, with mod_face=0..5, the vector will be
 * rotated to each face of a cube.  Rotations are:
 *   mod_face = 0 mod 3, +Z axis rotate
 *   mod_face = 1 mod 3, +X axis rotate
 *   mod_face = 2 mod 3, -Y axis rotate
 */
void
lib_rotate_cube_face(vec, major_axis, mod_face)
    COORD3 vec;
    int major_axis, mod_face;
{
    double swap;

    mod_face = (mod_face+major_axis) % 3 ;
    if (mod_face == 0) {
	swap	 = vec[X];
	vec[X] = -vec[Y];
	vec[Y] = swap;
    } else if (mod_face == 1) {
	swap	 = vec[Y];
	vec[Y] = -vec[Z];
	vec[Z] = swap;
    } else {
	swap	 = vec[X];
	vec[X] = -vec[Z];
	vec[Z] = swap;
    }
}

/*
 * Portable gaussian random number generator (from "Numerical Recipes", GASDEV)
 * Returns a uniform random deviate between 0.0 and 1.0.  'iseed' must be
 * less than M1 to avoid repetition, and less than (2**31-C1)/A1 [= 300718]
 * to avoid overflow.
 */
#define M1  134456
#define IA1   8121
#define IC1  28411
#define RM1 1.0/M1

double
lib_gauss_rand(iseed)
    long iseed;
{
    double fac, v1, v2, r;
    long     ix1, ix2;

    ix2 = iseed;
    do {
	ix1 = (IC1+ix2*IA1) % M1;
	ix2 = (IC1+ix1*IA1) % M1;
	v1 = ix1 * 2.0 * RM1 - 1.0;
	v2 = ix2 * 2.0 * RM1 - 1.0;
	r = v1*v1 + v2*v2;
    } while ( r >= 1.0 );

    fac = sqrt((double)(-2.0 * log((double)r) / r));
    return v1 * fac;
}
