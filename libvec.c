/*
 * libvec.c - a library of vector operations and a random number generator
 *
 * Author:  Eric Haines, 3D/Eye, Inc.
 *
 * Modified: 1 October 1992
 *           Alexander R. Enzmann
 *          Object generation routines split off to keep file size down
 *
 * Modified: 17 Jan 1993
 *         Eduard [esp] Schwan
 *           Removed unused local variables, added <stdlib.h> include for
 *          exit() call
 *
 * Modified: 1 November 1994
 *           Alexander R. Enzmann
 *          Added routines to invert matrices, transform normals, and
 *          determine rotate/scale/translate from a transform matrix.
 *          Modified computation of perspective view matrix to be a
 *          little cleaner.
 *
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
    mx[3][3] = 1.0;
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

#ifdef _DEBUG
static void
show_matrix(mat)
   MATRIX mat;
{
   int i, j;
   for (i=0;i<4;i++) {
      for (j=0;j<4;j++)
	 fprintf(stderr, "%7.4g ", mat[i][j]);
      fprintf(stderr, "\n");
      }
}
#endif

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
    double xscale, yscale, view_dist;
    COORD3 right, up_dir;
    COORD3 Va;
    MATRIX Tv, Tt, Tx;

    /* Change the view angle into radians */
    angle = PI * angle / 180.0;

    /* Set the hither clipping plane */
    view_dist = 0.001;

    /* Translate point of interest to the origin */
    SET_COORD3(Va, -from[X], -from[Y], -from[Z]);
    lib_create_translate_matrix(Tv, Va);

    /* Move the eye by the same amount */
    SUB3_COORD3(Va, at, from)

    /* Make sure this is a valid sort of setup */
    if (Va[X] == 0.0 && Va[Y] == 0.0 && Va[Z] == 0.0) {
      fprintf(stderr, "Degenerate perspective transformation\n");
     exit(1);
    }

    /* Get the up vector to be perpendicular to the view vector */
    lib_normalize_vector(Va);
    COPY_COORD3(up_dir, up);
    CROSS(right, up_dir, Va);
    lib_normalize_vector(right);
    CROSS(up_dir, Va, right);
    lib_normalize_vector(up_dir);

    /* Create the orthogonal view transformation */
    Tt[0][0] = right[0];
    Tt[1][0] = right[1];
    Tt[2][0] = right[2];
    Tt[3][0] = 0;

    Tt[0][1] = up_dir[0];
    Tt[1][1] = up_dir[1];
    Tt[2][1] = up_dir[2];
    Tt[3][1] = 0;

    Tt[0][2] = Va[0];
    Tt[1][2] = Va[1];
    Tt[2][2] = Va[2];
    Tt[3][2] = 0;

    Tt[0][3] = 0;
    Tt[1][3] = 0;
    Tt[2][3] = 0;
    Tt[3][3] = 1;
    lib_matrix_multiply(Tx, Tv, Tt);
    lib_copy_matrix(Tv, Tx);

    /* Now add in the perspective transformation */
    lib_create_identity_matrix(Tt);
    Tt[2][2] =  1.0 / (1.0 + view_dist);
    Tt[3][2] =  view_dist / (1.0 + view_dist);
    Tt[2][3] =  1.0;
    Tt[3][3] =  0.0;
    lib_matrix_multiply(Tx, Tv, Tt);
    lib_copy_matrix(Tv, Tx);

    /* Determine how much to scale things by */
    yscale = (double)yres / (2.0 * tan(angle/2.0));
    xscale = yscale * (double)xres / ((double)yres * aspect);
    SET_COORD3(Va, -xscale, -yscale, 1.0);
    lib_create_scale_matrix(Tt, Va);
    lib_matrix_multiply(Tx, Tv, Tt);
    lib_copy_matrix(Tv, Tx);

    /* Translate the points so that <0,0> is at the center of
       the screen */
    SET_COORD3(Va, xres/2, yres/2, 0.0);
    lib_create_translate_matrix(Tt, Va);

    /* We now have the final transformation matrix */
    lib_matrix_multiply(trans, Tv, Tt);
}

/*
 * Multiply a vector by a matrix.
 */
void
lib_transform_vector(vres, vec, mx)
    COORD3 vres, vec;
    MATRIX mx;
{
    COORD3 vtemp;
    vtemp[X] = vec[X]*mx[0][0] + vec[Y]*mx[1][0] + vec[Z]*mx[2][0];
    vtemp[Y] = vec[X]*mx[0][1] + vec[Y]*mx[1][1] + vec[Z]*mx[2][1];
    vtemp[Z] = vec[X]*mx[0][2] + vec[Y]*mx[1][2] + vec[Z]*mx[2][2];
    COPY_COORD3(vres, vtemp);
}

/*
 * Multiply a normal by a matrix (i.e. multiply by transpose).
 */
void
lib_transform_normal(vres, vec, mx)
    COORD3 vres, vec;
    MATRIX mx;
{
    COORD3 vtemp;
    vtemp[X] = vec[X]*mx[0][0] + vec[Y]*mx[0][1] + vec[Z]*mx[0][2];
    vtemp[Y] = vec[X]*mx[1][0] + vec[Y]*mx[1][1] + vec[Z]*mx[1][2];
    vtemp[Z] = vec[X]*mx[2][0] + vec[Y]*mx[2][1] + vec[Z]*mx[2][2];
    COPY_COORD3(vres, vtemp);
}

/*
 * Multiply a point by a matrix.
 */
void
lib_transform_point(vres, vec, mx)
    COORD3 vres, vec;
    MATRIX mx;
{
    COORD3 vtemp;
    vtemp[X] = vec[X]*mx[0][0] + vec[Y]*mx[1][0] + vec[Z]*mx[2][0] + mx[3][0];
    vtemp[Y] = vec[X]*mx[0][1] + vec[Y]*mx[1][1] + vec[Z]*mx[2][1] + mx[3][1];
    vtemp[Z] = vec[X]*mx[0][2] + vec[Y]*mx[1][2] + vec[Z]*mx[2][2] + mx[3][2];
    COPY_COORD3(vres, vtemp);
}

/*
 * Multiply a 4 element vector by a matrix.  Typically used for
 * homogenous transformation from world space to screen space.
 */
void
lib_transform_coord(vres, vec, mx)
    COORD4 vres, vec;
    MATRIX mx;
{
    COORD4 vtemp;
    vtemp[X] = vec[X]*mx[0][0]+vec[Y]*mx[1][0]+vec[Z]*mx[2][0]+vec[W]*mx[3][0];
    vtemp[Y] = vec[X]*mx[0][1]+vec[Y]*mx[1][1]+vec[Z]*mx[2][1]+vec[W]*mx[3][1];
    vtemp[Z] = vec[X]*mx[0][2]+vec[Y]*mx[1][2]+vec[Z]*mx[2][2]+vec[W]*mx[3][2];
    vtemp[W] = vec[X]*mx[0][3]+vec[Y]*mx[1][3]+vec[Z]*mx[2][3]+vec[W]*mx[3][3];
    COPY_COORD4(vres, vtemp);
}

/* Determinant of a 3x3 matrix */
static double
det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3)
   double a1, a2, a3;
   double b1, b2, b3;
   double c1, c2, c3;
{
    double ans;

    ans = a1 * (b2 * c3 - b3 * c2) -
	  b1 * (a2 * c3 - a3 * c2) +
	  c1 * (a2 * b3 - a3 * b2);
    return ans;
}

/* Adjoint of a 4x4 matrix - used in the computation of the inverse
   of a 4x4 matrix */
static void
adjoint(out_mat, in_mat)
   MATRIX out_mat, in_mat;
{
   double a1, a2, a3, a4, b1, b2, b3, b4;
   double c1, c2, c3, c4, d1, d2, d3, d4;

   a1 = in_mat[0][0]; b1 = in_mat[0][1];
   c1 = in_mat[0][2]; d1 = in_mat[0][3];
   a2 = in_mat[1][0]; b2 = in_mat[1][1];
   c2 = in_mat[1][2]; d2 = in_mat[1][3];
   a3 = in_mat[2][0]; b3 = in_mat[2][1];
   c3 = in_mat[2][2]; d3 = in_mat[2][3];
   a4 = in_mat[3][0]; b4 = in_mat[3][1];
   c4 = in_mat[3][2]; d4 = in_mat[3][3];


    /* row column labeling reversed since we transpose rows & columns */
   out_mat[0][0]  =   det3x3( b2, b3, b4, c2, c3, c4, d2, d3, d4);
   out_mat[1][0]  = - det3x3( a2, a3, a4, c2, c3, c4, d2, d3, d4);
   out_mat[2][0]  =   det3x3( a2, a3, a4, b2, b3, b4, d2, d3, d4);
   out_mat[3][0]  = - det3x3( a2, a3, a4, b2, b3, b4, c2, c3, c4);

   out_mat[0][1]  = - det3x3( b1, b3, b4, c1, c3, c4, d1, d3, d4);
   out_mat[1][1]  =   det3x3( a1, a3, a4, c1, c3, c4, d1, d3, d4);
   out_mat[2][1]  = - det3x3( a1, a3, a4, b1, b3, b4, d1, d3, d4);
   out_mat[3][1]  =   det3x3( a1, a3, a4, b1, b3, b4, c1, c3, c4);

   out_mat[0][2]  =   det3x3( b1, b2, b4, c1, c2, c4, d1, d2, d4);
   out_mat[1][2]  = - det3x3( a1, a2, a4, c1, c2, c4, d1, d2, d4);
   out_mat[2][2]  =   det3x3( a1, a2, a4, b1, b2, b4, d1, d2, d4);
   out_mat[3][2]  = - det3x3( a1, a2, a4, b1, b2, b4, c1, c2, c4);

   out_mat[0][3]  = - det3x3( b1, b2, b3, c1, c2, c3, d1, d2, d3);
   out_mat[1][3]  =   det3x3( a1, a2, a3, c1, c2, c3, d1, d2, d3);
   out_mat[2][3]  = - det3x3( a1, a2, a3, b1, b2, b3, d1, d2, d3);
   out_mat[3][3]  =   det3x3( a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

/* Determinant of a 4x4 matrix */
double
lib_matrix_det4x4(mat)
   MATRIX mat;
{
   double ans;
   double a1, a2, a3, a4, b1, b2, b3, b4, c1, c2, c3, c4, d1, d2, d3, d4;

   a1 = mat[0][0]; b1 = mat[0][1]; c1 = mat[0][2]; d1 = mat[0][3];
   a2 = mat[1][0]; b2 = mat[1][1]; c2 = mat[1][2]; d2 = mat[1][3];
   a3 = mat[2][0]; b3 = mat[2][1]; c3 = mat[2][2]; d3 = mat[2][3];
   a4 = mat[3][0]; b4 = mat[3][1]; c4 = mat[3][2]; d4 = mat[3][3];

   ans = a1 * det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4) -
	 b1 * det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4) +
	 c1 * det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4) -
	 d1 * det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
   return ans;
}

/* Find the inverse of a 4x4 matrix */
void
lib_invert_matrix(out_mat, in_mat)
   MATRIX out_mat, in_mat;
{
   int i, j;
   double det;

   adjoint(out_mat, in_mat);
   det = lib_matrix_det4x4(in_mat);
   if (fabs(det) < EPSILON) {
      lib_create_identity_matrix(out_mat);
      return;
      }
   for (i=0;i<4;i++)
      for (j=0;j<4;j++)
	 out_mat[i][j] /= det;
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
 * Multiply two 4x4 matrices.  Note that mxres had better not be
 * the same as either mx1 or mx2 or bad results will be returned.
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

    mod_face = (mod_face+major_axis) % 3;
    if (mod_face == 0) {
	swap   = vec[X];
	vec[X] = -vec[Y];
	vec[Y] = swap;
    } else if (mod_face == 1) {
	swap   = vec[Y];
	vec[Y] = -vec[Z];
	vec[Z] = swap;
    } else {
	swap   = vec[X];
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
