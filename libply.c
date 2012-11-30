/*
 * libply.c - a library of polygonized object output routines.
 *
 * Author:  Alexander Enzmann
 *
 */


/*-----------------------------------------------------------------*/
/* include section */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "lib.h"
#include "drv.h"


/*-----------------------------------------------------------------*/
/* defines/constants section */

#define VERT(i, a) ((verts[gPoly_vbuffer[i]])[a])


/*-----------------------------------------------------------------*/
/* Polygon stack for making PLG files */
object_ptr gPolygon_stack = NULL;

/* Storage for polygon indices */
unsigned int *gPoly_vbuffer = NULL;
int *gPoly_end = NULL;

/* Globals to determine which axes can be used to split the polygon */
int gPoly_Axis1 = 0;
int gPoly_Axis2 = 1;


/*-----------------------------------------------------------------*/
void
lib_output_polygon_cylcone(base_pt, apex_pt)
    COORD4 base_pt, apex_pt;
{
    double angle, delta_angle, divisor;
    COORD3 axis, dir, norm_axis, start_norm;
    COORD3 norm[4], vert[4], start_radius[4];
    MATRIX mx;
    int    i;

    SUB3_COORD3(axis, apex_pt, base_pt);
    COPY_COORD3(norm_axis, axis);
    lib_normalize_vector(norm_axis);

    SET_COORD3(dir, 0.0, 0.0, 1.0);
    CROSS(start_norm, axis, dir);
    divisor = lib_normalize_vector(start_norm);

    if (ABSOLUTE(divisor) < EPSILON2) {
	SET_COORD3(dir, 1.0, 0.0, 0.0);
	CROSS(start_norm, axis, dir);
	lib_normalize_vector(start_norm);
    }

    start_radius[0][X] = start_norm[X] * base_pt[W];
    start_radius[0][Y] = start_norm[Y] * base_pt[W];
    start_radius[0][Z] = start_norm[Z] * base_pt[W];
    ADD3_COORD3(vert[0], base_pt, start_radius[0]);

    start_radius[1][X] = start_norm[X] * apex_pt[W];
    start_radius[1][Y] = start_norm[Y] * apex_pt[W];
    start_radius[1][Z] = start_norm[Z] * apex_pt[W];
    ADD3_COORD3(vert[1], apex_pt, start_radius[1]);

    COPY_COORD3(norm[0], start_norm);
    COPY_COORD3(norm[1], start_norm);

    delta_angle = 2.0 * PI / (double)(2*gU_resolution);
    for (i=1,angle=delta_angle;i<=2*gU_resolution;++i,angle+=delta_angle) {
	lib_create_axis_rotate_matrix(mx, norm_axis, angle);
	lib_transform_vector(vert[2], start_radius[1], mx);
	ADD2_COORD3(vert[2], apex_pt);
	lib_transform_vector(norm[2], start_norm, mx);
	lib_output_polypatch(3, vert, norm);
	COPY_COORD3(vert[1], vert[2]);
	COPY_COORD3(norm[1], norm[2]);
	lib_transform_vector(vert[2], start_radius[0], mx);
	ADD2_COORD3(vert[2], base_pt);
	lib_output_polypatch(3, vert, norm);

	COPY_COORD3(vert[0], vert[2]);
	COPY_COORD3(norm[0], norm[2]);

	PLATFORM_MULTITASK();
    }
}

/*-----------------------------------------------------------------*/
static void
disc_evaluator(trans, theta, v, r, vert)
    MATRIX trans;
    double theta, v, r;
    COORD3 vert;
{
    COORD3 tvert;

    /* Compute the position of the point */
    SET_COORD3(tvert, (r + v) * cos(theta), (r + v) * sin(theta), 0.0);
    lib_transform_vector(vert, tvert, trans);
}

/*-----------------------------------------------------------------*/
void
lib_output_polygon_disc(center, normal, iradius, oradius)
    COORD3 center, normal;
    double iradius, oradius;
{
    double u, v, delta_u, delta_v;
    MATRIX mx, imx;
    int i, j;
    COORD3 norm, vert[4];

    COPY_COORD3(norm, normal);
    if ( lib_normalize_vector(norm) < EPSILON2) {
	fprintf(stderr, "Bad disc normal\n");
	exit(1);
    }
    lib_create_canonical_matrix(mx, imx, center, norm);
    delta_u = 2.0 * PI / (double)(4 * gU_resolution);
    delta_v = (oradius - iradius) / (double)(gV_resolution);

    /* Dump out polygons */
    for (i=0,u=0.0;i<4*gU_resolution;i++,u+=delta_u) {
	PLATFORM_MULTITASK();
	for (j=0,v=0.0;j<gV_resolution;j++,v+=delta_v) {
	    disc_evaluator(imx, u, v, iradius, vert[0]);
	    disc_evaluator(imx, u+delta_u, v, iradius, vert[1]);
	    disc_evaluator(imx, u+delta_u, v+delta_v, iradius, vert[2]);
	    lib_output_polygon(3, vert);
	    COPY_COORD3(vert[1], vert[2]);
	    disc_evaluator(imx, u, v+delta_v, iradius, vert[2]);
	    lib_output_polygon(3, vert);
	}
    }
}

/*-----------------------------------------------------------------*/
void
lib_output_polygon_sphere(center_pt)
    COORD4 center_pt;
{
    double  angle;
    COORD3  edge_norm[3], edge_pt[3];
    long    num_face, num_edge, num_tri, num_vert;
    COORD3  *x_axis, *y_axis, **pt;
    COORD3  mid_axis;
    MATRIX  rot_mx;
    long    u_pol, v_pol;

    /* Allocate storage for the polygon vertices */
    x_axis = (COORD3 *)malloc((gU_resolution+1) * sizeof(COORD3));
    y_axis = (COORD3 *)malloc((gV_resolution+1) * sizeof(COORD3));
    pt     = (COORD3 **)malloc((gU_resolution+1) * sizeof(COORD3 *));
    if (x_axis == NULL || y_axis == NULL || pt == NULL) {
	fprintf(stderr, "Failed to allocate polygon data\n");
	exit(1);
    }

    for (num_edge=0;num_edge<gU_resolution+1;num_edge++) {
	pt[num_edge] = (COORD3 *)malloc((gV_resolution+1) * sizeof(COORD3));
	if (pt[num_edge] == NULL) {
	    fprintf(stderr, "Failed to allocate polygon data\n");
	    exit(1);
	}
    }

    /* calculate axes used to find grid points */
    for (num_edge=0;num_edge<=gU_resolution;++num_edge) {
	angle = (PI/4.0) * (2.0*(double)num_edge/gU_resolution - 1.0);
	mid_axis[X] = 1.0; mid_axis[Y] = 0.0; mid_axis[Z] = 0.0;
	lib_create_rotate_matrix(rot_mx, Y_AXIS, angle);
	lib_transform_vector(x_axis[num_edge], mid_axis, rot_mx);
    }

    for (num_edge=0;num_edge<=gV_resolution;++num_edge) {
	angle = (PI/4.0) * (2.0*(double)num_edge/gV_resolution - 1.0);
	mid_axis[X] = 0.0; mid_axis[Y] = 1.0; mid_axis[Z] = 0.0;
	lib_create_rotate_matrix(rot_mx, X_AXIS, angle);
	lib_transform_vector(y_axis[num_edge], mid_axis, rot_mx);
    }

    /* set up grid of points on +Z sphere surface */
    for (u_pol=0;u_pol<=gU_resolution;++u_pol) {
	for (v_pol=0;v_pol<=gU_resolution;++v_pol) {
	    CROSS(pt[u_pol][v_pol], x_axis[u_pol], y_axis[v_pol]);
	    lib_normalize_vector(pt[u_pol][v_pol]);
	}
    }

    for (num_face=0;num_face<6;++num_face) {
	/* transform points to cube face */
	for (u_pol=0;u_pol<=gU_resolution;++u_pol) {
	    for (v_pol=0;v_pol<=gV_resolution;++v_pol) {
		lib_rotate_cube_face(pt[u_pol][v_pol], Z_AXIS, num_face);
	    }
	}

	/* output grid */
	for (u_pol=0;u_pol<gU_resolution;++u_pol) {
	    for (v_pol=0;v_pol<gV_resolution;++v_pol) {
		PLATFORM_MULTITASK();
		for (num_tri=0;num_tri<2;++num_tri) {
		    for (num_edge=0;num_edge<3;++num_edge) {
			num_vert = (num_tri*2 + num_edge) % 4;
			if (num_vert == 0) {
			    COPY_COORD3(edge_pt[num_edge], pt[u_pol][v_pol]);
			} else if ( num_vert == 1 ) {
			    COPY_COORD3(edge_pt[num_edge], pt[u_pol][v_pol+1]);
			} else if ( num_vert == 2 ) {
			    COPY_COORD3(edge_pt[num_edge],pt[u_pol+1][v_pol+1]);
			} else {
			    COPY_COORD3(edge_pt[num_edge], pt[u_pol+1][v_pol]);
			}
			COPY_COORD3(edge_norm[num_edge], edge_pt[num_edge]);
			edge_pt[num_edge][X] =
				edge_pt[num_edge][X] * center_pt[W] +
					       center_pt[X];
			edge_pt[num_edge][Y] =
				edge_pt[num_edge][Y] * center_pt[W] +
					       center_pt[Y];
			edge_pt[num_edge][Z] =
				edge_pt[num_edge][Z] * center_pt[W] +
					       center_pt[Z];

		    }
		    lib_output_polypatch(3, edge_pt, edge_norm);
		}
	    }
	}
    }

    /* Release any memory used */
    for (num_edge=0;num_edge<gU_resolution+1;num_edge++)
	free(pt[num_edge]);
    free(pt);
    free(y_axis);
    free(x_axis);
}


/*-----------------------------------------------------------------*/
void
lib_output_polygon_height(height, width, data, x0, x1, y0, y1, z0, z1)
    unsigned int height, width;
    float **data;
    double x0, x1, y0, y1, z0, z1;
{
    unsigned int i, j;
    double xdelta, zdelta;
    COORD3 verts[3];

#if defined (applec)
#pragma unused (y1)
#endif /* applec */

    xdelta = (x1 - x0) / (double)(width - 1);
    zdelta = (z1 - z0) / (double)(height - 1);
    for (i=0;i<height-1;i++) {
	for (j=0;j<width-1;j++) {
	    PLATFORM_MULTITASK();
	    SET_COORD3(verts[0], x0 + j * xdelta, y0 + data[i][j],
		       z0 + i * zdelta);
	    SET_COORD3(verts[1], x0 + (j+1) * xdelta, y0 + data[i][j+1],
		       z0 + i * zdelta);
	    SET_COORD3(verts[2], x0 + (j+1) * xdelta, y0 + data[i+1][j+1],
		       z0 + (i + 1) * zdelta);
	    lib_output_polygon(3, verts);
	    COPY_COORD3(verts[1], verts[2]);
	    SET_COORD3(verts[2], x0 + j * xdelta, y0 + data[i+1][j],
		       z0 + (i + 1) * zdelta);
	    lib_output_polygon(3, verts);
	}
    }
}

/*-----------------------------------------------------------------*/
static void
torus_evaluator(trans, theta, phi, r0, r1, vert, norm)
    MATRIX trans;
    double theta, phi, r0, r1;
    COORD3 vert, norm;
{
    COORD3 v0, v1, tvert, tnorm;

    /* Compute the position of the point */
    SET_COORD3(tvert, (r0 + r1 * sin(theta)) * cos(phi),
		      (r0 + r1 * sin(theta)) * sin(phi),
		      r1 * cos(theta));
    /* Compute the normal at that point */
    SET_COORD3(v0, r1*cos(theta)*cos(phi),
		   r1*cos(theta)*sin(phi),
		  -r1*sin(theta));
    SET_COORD3(v1,-(r0+r1*sin(theta))*sin(phi),
		   (r0+r1*sin(theta))*cos(phi),
		   0.0);
    CROSS(tnorm, v0, v1);
    lib_normalize_vector(tnorm);
    lib_transform_point(vert, tvert, trans);
    lib_transform_vector(norm, tnorm, trans);
}

/*-----------------------------------------------------------------*/
void
lib_output_polygon_torus(center, normal, iradius, oradius)
    COORD3 center, normal;
    double iradius, oradius;
{
    double u, v, delta_u, delta_v;
    MATRIX mx, imx;
    int i, j;
    COORD3 vert[4], norm[4];

    if ( lib_normalize_vector(normal) < EPSILON2) {
	fprintf(stderr, "Bad torus normal\n");
	exit(1);
    }
    lib_create_canonical_matrix(mx, imx, center, normal);
    delta_u = 2.0 * PI / (double)gU_resolution;
    delta_v = 2.0 * PI / (double)gV_resolution;

    /* Dump out polygons */
    for (i=0,u=0.0;i<gU_resolution;i++,u+=delta_u) {
	PLATFORM_MULTITASK();
	for (j=0,v=0.0;j<gV_resolution;j++,v+=delta_v) {
	    torus_evaluator(imx, u, v, iradius, oradius, vert[0], norm[0]);
	    torus_evaluator(imx, u, v+delta_v, iradius, oradius,
			vert[1], norm[1]);
	    torus_evaluator(imx, u+delta_u, v+delta_v,
			iradius, oradius, vert[2], norm[2]);
	    lib_output_polypatch(3, vert, norm);
	    COPY_COORD3(vert[1], vert[2]);
	    COPY_COORD3(norm[1], norm[2]);
	    torus_evaluator(imx, u+delta_u, v, iradius, oradius,
			vert[2], norm[2]);
	    lib_output_polypatch(3, vert, norm);
	}
    }
}
/*-----------------------------------------------------------------*/
/* Generate a box as a set of 4-sided polygons */
void
lib_output_polygon_box(p1, p2)
    COORD3 p1, p2;
{
    COORD3 box_verts[4];

    /* Sides */
    SET_COORD3(box_verts[0], p1[X], p1[Y], p1[Z]);
    SET_COORD3(box_verts[1], p1[X], p2[Y], p1[Z]);
    SET_COORD3(box_verts[2], p1[X], p2[Y], p2[Z]);
    SET_COORD3(box_verts[3], p1[X], p1[Y], p2[Z]);
    lib_output_polygon(4, box_verts);
    SET_COORD3(box_verts[3], p2[X], p1[Y], p1[Z]);
    SET_COORD3(box_verts[2], p2[X], p2[Y], p1[Z]);
    SET_COORD3(box_verts[1], p2[X], p2[Y], p2[Z]);
    SET_COORD3(box_verts[0], p2[X], p1[Y], p2[Z]);
    lib_output_polygon(4, box_verts);

    /* Front/Back */
    SET_COORD3(box_verts[0], p1[X], p1[Y], p1[Z]);
    SET_COORD3(box_verts[1], p1[X], p2[Y], p1[Z]);
    SET_COORD3(box_verts[2], p2[X], p2[Y], p1[Z]);
    SET_COORD3(box_verts[3], p2[X], p1[Y], p1[Z]);
    lib_output_polygon(4, box_verts);
    SET_COORD3(box_verts[3], p1[X], p1[Y], p2[Z]);
    SET_COORD3(box_verts[2], p1[X], p2[Y], p2[Z]);
    SET_COORD3(box_verts[1], p2[X], p2[Y], p2[Z]);
    SET_COORD3(box_verts[0], p2[X], p1[Y], p2[Z]);
    lib_output_polygon(4, box_verts);

    /* Top/Bottom */
    SET_COORD3(box_verts[0], p1[X], p1[Y], p1[Z]);
    SET_COORD3(box_verts[1], p1[X], p1[Y], p2[Z]);
    SET_COORD3(box_verts[2], p2[X], p1[Y], p2[Z]);
    SET_COORD3(box_verts[3], p2[X], p1[Y], p1[Z]);
    lib_output_polygon(4, box_verts);
    SET_COORD3(box_verts[3], p1[X], p2[Y], p1[Z]);
    SET_COORD3(box_verts[2], p1[X], p2[Y], p2[Z]);
    SET_COORD3(box_verts[1], p2[X], p2[Y], p2[Z]);
    SET_COORD3(box_verts[0], p2[X], p2[Y], p1[Z]);
    lib_output_polygon(4, box_verts);
}


/*-----------------------------------------------------------------*/
/* Given a polygon defined by vertices in verts, determine which of the
   components of the vertex correspond to useful x and y coordinates - with
   these we can pretend the polygon is 2D to do our work on it. */
static void
find_axes(verts)
    COORD3 *verts;
{
    double P1[3], P2[3], x, y, z;

    P1[0] = VERT(1, 0) - VERT(0, 0);
    P1[1] = VERT(1, 1) - VERT(0, 1);
    P1[2] = VERT(1, 2) - VERT(0, 2);

    P2[0] = VERT(2, 0) - VERT(0, 0);
    P2[1] = VERT(2, 1) - VERT(0, 1);
    P2[2] = VERT(2, 2) - VERT(0, 2);

    /* Cross product - don't need to normalize cause we're only interested
      in the size of the components */
    x = fabs(P1[1] * P2[2] - P1[2] * P2[1]);
    y = fabs(P1[2] * P2[0] - P1[0] * P2[2]);
    z = fabs(P1[0] * P2[1] - P1[1] * P2[0]);

    if (x > y && x > z) {
	gPoly_Axis1 = 1;
	gPoly_Axis2 = 2;
    } else if (y > x && y > z) {
	gPoly_Axis1 = 0;
	gPoly_Axis2 = 2;
    } else {
	gPoly_Axis1 = 0;
	gPoly_Axis2 = 1;
    }
}

/*-----------------------------------------------------------------*/
/* Find the left most vertex in the polygon that has vertices m ... n. */
static unsigned int
leftmost_vertex(m, n, verts)
    unsigned int m, n;
    COORD3 *verts;
{
    unsigned int l, i;
    double x;

    /* Assume the first vertex is the farthest to the left */
    l = m;
    x = VERT(m, gPoly_Axis1);

    /* Now see if any of the others are farther to the left */
    for (i=m+1;i<=n;i++) {
       if (VERT(i, gPoly_Axis1) < x) {
	   l = i;
	   x = VERT(i, gPoly_Axis1);
       }
    }
    return l;
}

/*-----------------------------------------------------------------*/
/* Given the leftmost vertex in a polygon, this routine finds another vertex
   can be used to safely split the polygon. */
static unsigned int
split_vertex(l, la, lb, m, n, verts)
    unsigned int l, la, lb, m, n;
    COORD3 *verts;
{
    unsigned int t, k, lpu, lpl;
    double yu, yl;

    yu = MAX(VERT(l, gPoly_Axis2), MAX(VERT(la, gPoly_Axis2), VERT(lb, gPoly_Axis2)));
    yl = MIN(VERT(l, gPoly_Axis2), MIN(VERT(la, gPoly_Axis2), VERT(lb, gPoly_Axis2)));
    if (VERT(lb, gPoly_Axis2) > VERT(la, gPoly_Axis2)) {
	lpu = lb;
	lpl = la;
    } else {
	lpu = la;
	lpl = lb;
    }
    t = (VERT(lb, gPoly_Axis1) > VERT(la, gPoly_Axis1) ? lb : la);
    for (k=m;k<n;k++) {
	if (k != la && k != l && k != lb) {
	    if (VERT(k, gPoly_Axis2) <= yu && VERT(k, gPoly_Axis2) >= yl) {
		if (VERT(k, gPoly_Axis1) < VERT(t, gPoly_Axis1) &&
		    ((VERT(k, gPoly_Axis2) - VERT(l, gPoly_Axis2)) *
		     (VERT(lpu, gPoly_Axis1) - VERT(l, gPoly_Axis1))) <=
		    ((VERT(lpu, gPoly_Axis2) - VERT(l, gPoly_Axis2)) *
		     (VERT(k, gPoly_Axis1) - VERT(l, gPoly_Axis1)))) {
		    if (((VERT(k, gPoly_Axis2) - VERT(l, gPoly_Axis2)) *
			  (VERT(lpl, gPoly_Axis1) - VERT(l, gPoly_Axis1))) >=
			 ((VERT(lpl, gPoly_Axis2) - VERT(l, gPoly_Axis2)) *
			 (VERT(k, gPoly_Axis1) - VERT(l, gPoly_Axis1)))) {
			t = k;
		    }
		}
	    }
	}
    }
    return t;
}

/*-----------------------------------------------------------------*/
/* Test polygon vertices to see if they are linear */
static int
linear_vertices(m, n, verts)
    unsigned int m, n;
    COORD3 *verts;
{
#if defined (applec)
#pragma unused (m,n,verts)
#endif /* applec */
    /* Not doing anything right now */
    return 0;
}

/*-----------------------------------------------------------------*/
/* Shift vertex indices around to make two polygons out of one. */
static void
perform_split(m, m1, n, n1)
    int n, n1, m, m1;
{
    int i, j, k;

    k = n + 3 - m;
    /* Move the new polygon up over the place the current one sits */
    for (j=m1;j<=n1;j++) gPoly_vbuffer[j+k] = gPoly_vbuffer[j];

    /* Move top part of remaining polygon */
    for (j=n;j>=n1;j--) gPoly_vbuffer[j+2] = gPoly_vbuffer[j];

    /* Move bottom part of remaining polygon */
    k = n1 - m1 + 1;
    for (j=m1;j>=m;j--) gPoly_vbuffer[j+k] = gPoly_vbuffer[j];

    /* Copy the new polygon so that it sits before the remaining polygon */
    i = n + 3 - m;
    k = m - m1;
    for (j=m1;j<=n1;j++) gPoly_vbuffer[j+k] = gPoly_vbuffer[j+i];
}

/*-----------------------------------------------------------------*/
/* Copy an indirectly referenced triangle into the output triangle buffer */
static void
add_new_triangle(m, verts, norms, out_cnt, out_verts, out_norms)
    unsigned int m, *out_cnt;
    COORD3 *verts, *norms, **out_verts, **out_norms;
{
    if (out_verts != NULL) {
	COPY_COORD3(out_verts[*out_cnt][0], verts[gPoly_vbuffer[m]]);
	COPY_COORD3(out_verts[*out_cnt][1], verts[gPoly_vbuffer[m+1]]);
	COPY_COORD3(out_verts[*out_cnt][2], verts[gPoly_vbuffer[m+2]]);
    }
    if (out_norms != NULL) {
	COPY_COORD3(out_norms[*out_cnt][0], norms[gPoly_vbuffer[m]]);
	COPY_COORD3(out_norms[*out_cnt][1], norms[gPoly_vbuffer[m+1]]);
	COPY_COORD3(out_norms[*out_cnt][2], norms[gPoly_vbuffer[m+2]]);
    }
    *out_cnt += 1;
}

/*-----------------------------------------------------------------*/
static void
split_buffered_polygon(cnt, verts, norms, out_cnt, out_verts, out_norms)
    unsigned int cnt, *out_cnt;
    COORD3 *verts, *norms, **out_verts, **out_norms;
{
    unsigned int i, m, m1, n, n1;
    unsigned int l, la, lb, ls;

    /* No triangles to start with */
    *out_cnt = 0;

    /* Initialize the polygon splitter */
    gPoly_end[0] = -1;
    gPoly_end[1] = cnt-1;

    /* Split and push polygons until they turn into triangles */
    for (i=1;i>0;) {
	m = gPoly_end[i-1] + 1;
	n = gPoly_end[i];
	if (n - m == 2) {
	    if (!linear_vertices(m, n, verts)) {
		add_new_triangle(m, verts, norms, out_cnt,
			out_verts, out_norms);
	    }
	    i = i - 1;
	} else {
	    l = leftmost_vertex(m, n, verts);
	    la = (l == n ? m : l + 1);
	    lb = (l == m ? n : l - 1);
	    ls = split_vertex(l, la, lb, m, n, verts);
	    if (ls == la || ls == lb) {
		m1 = (la < lb ? la : lb);
		n1 = (la > lb ? la : lb);
	    } else {
		m1 = (l < ls ? l : ls);
		n1 = (l > ls ? l : ls);
	    }
	    perform_split(m, m1, n, n1);
	    gPoly_end[i++] = m + n1 - m1;
	    gPoly_end[i] = n + 2;
	}
    }
}

/*-----------------------------------------------------------------*/
/*
 * Split an arbitrary polygon into triangles.
 */
static void
split_polygon(n, vert, norm)
    unsigned int n;
    COORD3 *vert, *norm;
{
    COORD4 tvert[3], v0, v1;
    COORD3 **out_verts, **out_norms;
    int i, ii, j ;
    unsigned int t, out_n;
    object_ptr new_object;

    /* Can't split a NULL vertex list */
    if (vert == NULL) return;
    if (gPoly_vbuffer == NULL) { /* [esp] Added error */
	lib_storage_initialize();
	/* [are] removed error, go and initialize if it hasn't been done. */
    }

    /* Allocate space to hold the intermediate polygon stacks */
    out_verts = (COORD3 **)malloc((n - 2) * sizeof(COORD3 *));
    if (norm != NULL)
	out_norms = (COORD3 **)malloc((n - 2) * sizeof(COORD3 *));
    else
	out_norms = NULL;
    for (i=0;i<n-2;i++) {
	out_verts[i] = (COORD3 *)malloc(3 * sizeof(COORD3));
	if (norm != NULL)
	    out_norms[i] = (COORD3 *)malloc(3 * sizeof(COORD3));
    }

    /* Start with a strict identity of vertices in verts and vertices in
       the polygon buffer */
    for (i=0;i<n;i++) gPoly_vbuffer[i] = i;

    /* Make sure we know which axes to look at */
    find_axes(vert);

    out_n = 0;
    split_buffered_polygon(n, vert, norm, &out_n, out_verts, out_norms);

    /* Now output the triangles that we generated */
    for (t=0;t<out_n;t++) {
	PLATFORM_MULTITASK();
	if (gRT_out_format == OUTPUT_DELAYED || gRT_out_format == OUTPUT_PLG) {
	    /* Save all the pertinent information */
	    new_object = (object_ptr)malloc(sizeof(struct object_struct));
	    if (new_object == NULL) return;
	    if (norm == NULL) {
		new_object->object_type  = POLYGON_OBJ;
		new_object->object_data.polygon.tot_vert = 3;
		new_object->object_data.polygon.vert =
			(COORD3 *)malloc(3 * sizeof(COORD3));
		if (new_object->object_data.polygon.vert == NULL) return;
	    } else {
		new_object->object_type  = POLYPATCH_OBJ;
		new_object->object_data.polypatch.tot_vert = 3;
		new_object->object_data.polypatch.vert =
			(COORD3 *)malloc(3 * sizeof(COORD3));
		if (new_object->object_data.polypatch.vert == NULL) return;
		new_object->object_data.polypatch.norm =
			(COORD3 *)malloc(3 * sizeof(COORD3));
		if (new_object->object_data.polypatch.norm == NULL) return;
	    }
	    new_object->curve_format = OUTPUT_PATCHES;
	    new_object->surf_index   = gTexture_count;
	    for (i=0;i<3;i++) {
		if (norm == NULL)
		    COPY_COORD3(new_object->object_data.polygon.vert[i],
			    out_verts[t][i])
		else {
		    COPY_COORD3(new_object->object_data.polypatch.vert[i],
			    out_verts[t][i])
		    COPY_COORD3(new_object->object_data.polypatch.norm[i],
			    out_norms[t][i])
		}
	    }
	    new_object->next_object = gLib_objects;
	    gLib_objects = new_object;
	} else {
	    switch (gRT_out_format) {
		case OUTPUT_VIDEO:
		    /* First make sure the display has been opened for
		     * drawing
		     */
		    if (!gView_init_flag) {
			lib_create_view_matrix(gViewpoint.tx, gViewpoint.from, gViewpoint.at,
					       gViewpoint.up, gViewpoint.resx, gViewpoint.resy,
					       gViewpoint.angle, gViewpoint.aspect);
			display_init(gViewpoint.resx, gViewpoint.resy, gBkgnd_color);
			gView_init_flag = 1;
		    }
		    /* Step through each segment of the polygon, projecting it
		       onto the screen. */
		    for (i=0;i<3;i++) {
			COPY_COORD3(tvert[0], out_verts[t][i]);
			tvert[0][W] = 1.0;
			lib_transform_coord(v0, tvert[0], gViewpoint.tx);
			COPY_COORD3(tvert[1], out_verts[t][(i+1)%3]);
			tvert[1][W] = 1.0;
			lib_transform_coord(v1, tvert[1], gViewpoint.tx);
			/* Do the perspective transform on the points */
			v0[X] /= v0[W]; v0[Y] /= v0[W];
			v1[X] /= v1[W]; v1[Y] /= v1[W];
			if (lib_clip_to_box(v0, v1, gView_bounds))
			    display_line((int)v0[X], (int)v0[Y],
					(int)v1[X], (int)v1[Y], gFgnd_color);
		    }
		    break;

		case OUTPUT_NFF:
		    if (norm == NULL) {
			fprintf(gOutfile, "p 3\n");
			for (i=0;i<3;++i) {
			    fprintf(gOutfile, "%g %g %g\n",
				    out_verts[t][i][X], out_verts[t][i][Y],
				    out_verts[t][i][Z]);
			}
		    } else {
			fprintf(gOutfile, "pp 3\n");
			for (i=0;i<3;++i) {
			    fprintf(gOutfile, "%g %g %g %g %g %g\n",
				    out_verts[t][i][X], out_verts[t][i][Y],
				    out_verts[t][i][Z], out_norms[t][i][X],
				    out_norms[t][i][Y], out_norms[t][i][Z]);
			}
		    }
		   break;

		case OUTPUT_POVRAY_10:
		case OUTPUT_POVRAY_20:
		    tab_indent();
		    fprintf(gOutfile, "object {\n");
		    tab_inc();

		    tab_indent();
		    if (norm == NULL)
			fprintf(gOutfile, "triangle {\n");
		    else
			fprintf(gOutfile, "smooth_triangle {\n");
		    tab_inc();

		    for (i=0;i<3;++i) {
			tab_indent();
			if (gRT_out_format == OUTPUT_POVRAY_10) {
			    fprintf(gOutfile, "<%g %g %g>",
				   out_verts[t][i][X],
				   out_verts[t][i][Y],
				   out_verts[t][i][Z]);
			    if (norm != NULL)
				fprintf(gOutfile, " <%g %g %g>",
				       out_norms[t][i][X],
				       out_norms[t][i][Y],
				       out_norms[t][i][Z]);
			} else {
			    fprintf(gOutfile, "<%g, %g, %g>",
				   out_verts[t][i][X],
				   out_verts[t][i][Y],
				   out_verts[t][i][Z]);
			    if (norm != NULL)
			       fprintf(gOutfile, " <%g, %g, %g>",
				      out_norms[t][i][X],
				      out_norms[t][i][Y],
				      out_norms[t][i][Z]);
			    if (i < 2)
			       fprintf(gOutfile, ",");
			}
			fprintf(gOutfile, "\n");
		    } /*for*/

		    tab_dec();
		    tab_indent();
		    fprintf(gOutfile, "} // tri\n");

		    if (gTexture_name != NULL) {
		       tab_indent();
		       fprintf(gOutfile, "texture { %s }\n", gTexture_name);
		    }

		    tab_dec();
		    tab_indent();
		    fprintf(gOutfile, "} // object\n");

		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_POLYRAY:
		    if (norm == NULL) {
			tab_indent();
			fprintf(gOutfile, "object { polygon 3,");
			for (i=0;i<3;i++) {
			    fprintf(gOutfile, " <%g, %g, %g>",
				    out_verts[t][i][X], out_verts[t][i][Y],
				    out_verts[t][i][Z]);
			    if (i < 2)
				fprintf(gOutfile, ", ");
			}
		    } else {
			tab_indent();
			fprintf(gOutfile, "object { patch ");
			for (i=0;i<3;i++) {
			    fprintf(gOutfile, " <%g, %g, %g>, <%g, %g, %g>",
				    out_verts[t][i][X], out_verts[t][i][Y],
				    out_verts[t][i][Z], out_norms[t][i][X],
				    out_verts[t][i][Y], out_norms[t][i][Z]);
			    if (i < 2)
				fprintf(gOutfile, ", ");
			}
		    }
		    if (gTexture_name != NULL)
			fprintf(gOutfile, " %s", gTexture_name);
		    fprintf(gOutfile, " }\n");
		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_VIVID:
		    if (norm == NULL) {
			tab_indent();
			fprintf(gOutfile, "polygon { points 3 ");
			for (i=0;i<3;i++) {
			    fprintf(gOutfile, " vertex %g %g %g ",
				    out_verts[t][i][X], out_verts[t][i][Y],
				    out_verts[t][i][Z]);
			}
		    } else {
			fprintf(gOutfile, "patch {");
			for (i=0;i<3;++i) {
			    fprintf(gOutfile,
				    " vertex %g %g %g  normal %g %g %g ",
				    out_verts[t][i][X], out_verts[t][i][Y],
				    out_verts[t][i][Z], out_norms[t][i][X],
				    out_norms[t][i][Y], out_norms[t][i][Z]);
			}
		    }
		    fprintf(gOutfile, " }\n");
		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_QRT:
		    /* Doesn't matter if there are vertex normals,
		     * QRT can't use them.
		     */
		    fprintf(gOutfile, "TRIANGLE ( ");
		    fprintf(gOutfile, "loc = (%g, %g, %g), ",
			      out_verts[t][0][X], out_verts[t][0][Y],
			      out_verts[t][0][Z]);
		    fprintf(gOutfile, "vect1 = (%g, %g, %g), ",
			      out_verts[t][1][X] - out_verts[t][0][X],
			      out_verts[t][1][Y] - out_verts[t][0][Y],
			      out_verts[t][1][Z] - out_verts[t][0][Z]);
		    fprintf(gOutfile, "vect2 = (%g, %g, %g) ",
			      out_verts[t][2][X] - out_verts[t][0][X],
			      out_verts[t][2][Y] - out_verts[t][0][Y],
			      out_verts[t][2][Z] - out_verts[t][0][Z]);
		    fprintf(gOutfile, " );\n");
		    break;

		case OUTPUT_RAYSHADE:
		    fprintf(gOutfile, "triangle ");
		    if (gTexture_name != NULL)
			fprintf(gOutfile, "%s ", gTexture_name);
		    for (i=0;i<3;i++) {
			fprintf(gOutfile, "%g %g %g ",
				out_verts[t][i][X], out_verts[t][i][Y],
				out_verts[t][i][Z]);
			if (norm != NULL)
			    fprintf(gOutfile, "%g %g %g ",
				    out_norms[t][i][X], out_norms[t][i][Y],
				    out_norms[t][i][Z]);
		    }
		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_ART:
		    tab_indent();
		    fprintf(gOutfile, "polygon {\n");
		    tab_inc();

		    tab_indent();
		    for (i=0;i<3;i++) {
			tab_indent();
			fprintf(gOutfile, "vertex(%f, %f, %f)",
				out_verts[t][i][X], out_verts[t][i][Y],
				out_verts[t][i][Z]);
			if (norm != NULL)
			    fprintf(gOutfile, ", (%f, %f, %f)\n",
				    out_norms[t][i][X], out_norms[t][i][Y],
				    out_norms[t][i][Z]);
			else
			    fprintf(gOutfile, "\n");
		    }
		    tab_dec();
		    tab_indent();
		    fprintf(gOutfile, "}\n");
		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_RTRACE:
		    if (norm == NULL) {
			fprintf(gOutfile, "5 %d %g 0 0 0 1 1 1 -\n",
				gTexture_count, gTexture_ior);
			fprintf(gOutfile, "3 1 2 3\n\n");
		    } else {
			fprintf(gOutfile, "6 %d %g 0 0 0 1 1 1 -\n",
				gTexture_count, gTexture_ior);
		    }
		    for (i=0;i<3;i++) {
			fprintf(gOutfile, "%g %g %g",
				out_verts[t][i][X], out_verts[t][i][Y],
				out_verts[t][i][Z]);
			if (norm != NULL) {
			    fprintf(gOutfile, " %g %g %g",
				    out_norms[t][i][X], out_norms[t][i][Y],
				    out_norms[t][i][Z]);
			}
			fprintf(gOutfile, "\n");
		    }
		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_RAWTRI:
		    for (i=0;i<3;++i) {
			fprintf(gOutfile, "%-10.5g %-10.5g %-10.5g  ",
				out_verts[t][i][X], out_verts[t][i][Y],
				out_verts[t][i][Z]);
		    }

#ifdef RAWTRI_WITH_TEXTURES
		    /* raw triangle format extension to do textured raw
		     * triangles */
		    if (gTexture_name != NULL)
			fprintf(gOutfile, "%s", gTexture_name);
		    else
			/* for lack of a better name */
			fprintf(gOutfile, "texNone");
#endif /* RAWTRI_WITH_TEXTURES */

		    fprintf(gOutfile, "\n");
		    break;

		case OUTPUT_RIB:
		    /* The order of the vertices has to be inverted for the
		       LH system */
		    tab_indent();
		    fprintf(gOutfile, "Polygon \"P\" [\n");
		    tab_inc();
		    for (i=2;i>=0;i--)
		    {
			tab_indent();
			fprintf(gOutfile, "%#g %#g %#g\n",
			      out_verts[t][i][X], out_verts[t][i][Y],
			      out_verts[t][i][Z]);
		    }
		    if (norm != NULL)
		    {
			tab_dec();
			tab_indent();
			tab_inc();
			fprintf(gOutfile, "]  \"N\" [\n");
			for (i=2;i>=0;i--)
			{
			    /* Normals are also inverted in LH */
			    tab_indent();
			    fprintf(gOutfile, "%#g %#g %#g\n",
				     -out_norms[t][i][X], -out_norms[t][i][Y],
				     -out_norms[t][i][Z]);
			}
		    }
		    tab_dec();
		    tab_indent();
		    fprintf(gOutfile, "]\n");
		    break;

		case OUTPUT_DXF:
		    fprintf(gOutfile, "  0\n3DFACE\n  8\n0----\n" ) ;
		    for (i=0;i<4;++i) {
			ii = (i == 3) ? 2 : i ;
			for (j=0;j<3;++j) {
			    fprintf(gOutfile, " %d%d\n%0.4f\n",j+1,i,
				    out_verts[t][ii][j] ) ;
			}
		    }
		    break;

	    } /* switch */
	} /* else !OUTPUT_DELAYED */
    } /* else for loop */

    /* Clean up intermediate storage */
    for (i=0;i<n-2;i++) {
	free(out_verts[i]);
	if (norm != NULL)
	    free(out_norms[i]);
    }

    free(out_verts);
    if (out_norms != NULL) free(out_norms);
}

/*-----------------------------------------------------------------*/
/*
 * Output polygon.  A polygon is defined by a set of vertices.  With these
 * databases, a polygon is defined to have all points coplanar.  A polygon has
 * only one side, with the order of the vertices being counterclockwise as you
 * face the polygon (right-handed coordinate system).
 *
 * The output format is always:
 *     "p" total_vertices
 *     vert1.x vert1.y vert1.z
 *     [etc. for total_vertices polygons]
 *
 */
void
lib_output_polygon(tot_vert, vert)
    int tot_vert;
    COORD3 vert[];
{
    object_ptr new_object;
    int num_vert, i, j;
    COORD3 x;
    COORD4 tvert[3], v0, v1;

    /* First lets do a couple of checks to see if this is a valid polygon */
    for (i=0;i<tot_vert;) {
	/* If there are two adjacent coordinates that degenerate then
	   collapse them down to one */
	SUB3_COORD3(x, vert[i], vert[(i+1)%tot_vert]);
	if (lib_normalize_vector(x) < EPSILON2) {
	    for (j=i;j<tot_vert-1;j++)
		memcpy(&vert[j], &vert[j+1], sizeof(COORD4));
	    tot_vert--;
	}
	else {
	    i++;
	}
    }

    if (tot_vert < 3)
	/* No such thing as a poly that only has two sides */
	return;

    if (gRT_out_format == OUTPUT_DELAYED) {
	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	new_object->object_data.polygon.vert =
		(COORD3 *)malloc(tot_vert * sizeof(COORD3));
	if (new_object == NULL || new_object->object_data.polygon.vert == NULL)
	    /* Quietly fail */
	    return;
	new_object->object_type  = POLYGON_OBJ;
	new_object->curve_format = OUTPUT_PATCHES;
	new_object->surf_index   = gTexture_count;
	new_object->object_data.polygon.tot_vert = tot_vert;
	for (i=0;i<tot_vert;i++)
	    COPY_COORD3(new_object->object_data.polygon.vert[i], vert[i])
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;
    } else {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
		/* First make sure the display has been opened for drawing */
		if (!gView_init_flag) {
		    lib_create_view_matrix(gViewpoint.tx, gViewpoint.from, gViewpoint.at,
					   gViewpoint.up, gViewpoint.resx, gViewpoint.resy,
					   gViewpoint.angle, gViewpoint.aspect);
		    display_init(gViewpoint.resx, gViewpoint.resy, gBkgnd_color);
		    gView_init_flag = 1;
		}
		/* Step through each segment of the polygon, projecting it
		    onto the screen. */
		for (i=0;i<tot_vert;i++) {
		    COPY_COORD3(tvert[0], vert[i]);
		    tvert[0][W] = 1.0;
		    lib_transform_coord(v0, tvert[0], gViewpoint.tx);
		    COPY_COORD3(tvert[1], vert[(i+1)%tot_vert]);
		    tvert[1][W] = 1.0;
		    lib_transform_coord(v1, tvert[1], gViewpoint.tx);
		    /* Do the perspective transform on the points */
		    v0[X] /= v0[W]; v0[Y] /= v0[W];
		    v1[X] /= v1[W]; v1[Y] /= v1[W];
		    if (lib_clip_to_box(v0, v1, gView_bounds))
			display_line((int)v0[X], (int)v0[Y],
				(int)v1[X], (int)v1[Y], gFgnd_color);
		}
		break;

	    case OUTPUT_NFF:
		fprintf(gOutfile, "p %d\n", tot_vert);
		for (num_vert=0;num_vert<tot_vert;++num_vert)
		    fprintf(gOutfile, "%g %g %g\n",
			    vert[num_vert][X],
			    vert[num_vert][Y],
			    vert[num_vert][Z]);
		break;

	    case OUTPUT_POVRAY_10:
	    case OUTPUT_POVRAY_20:
	    case OUTPUT_QRT:
	    case OUTPUT_PLG:
	    case OUTPUT_RAWTRI:
	    case OUTPUT_DXF:
		/*
		These renderers don't do arbitrary polygons, split the polygon
		into triangles for output
		*/
		split_polygon(tot_vert, vert, NULL);
		break;

	    case OUTPUT_POLYRAY:
		tab_indent();
		fprintf(gOutfile, "object { polygon %d,", tot_vert);
		for (num_vert = 0; num_vert < tot_vert; num_vert++) {
		    fprintf(gOutfile, " <%g, %g, %g>",
			     vert[num_vert][X],
			     vert[num_vert][Y],
			     vert[num_vert][Z]);
		    if (num_vert < tot_vert-1)
			fprintf(gOutfile, ", ");
		}
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_VIVID:
		tab_indent();
		fprintf(gOutfile, "polygon { points %d ", tot_vert);
		for (num_vert = 0; num_vert < tot_vert; num_vert++) {
		    /* Vivid has problems with very long input lines, so in
		     * order to handle polygons with many vertices, we split
		     * the vertices one to a line.
		     */
		    fprintf(gOutfile, " vertex %g %g %g \n",
			     vert[num_vert][X],
			     vert[num_vert][Y],
			     vert[num_vert][Z]);
		}
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RAYSHADE:
		fprintf(gOutfile, "polygon ");
		if (gTexture_name != NULL)
		    fprintf(gOutfile, "%s ", gTexture_name);
		for (num_vert=0;num_vert<tot_vert;num_vert++) {
		    if (!(num_vert%3)) fprintf(gOutfile, "\n");
		    fprintf(gOutfile, "%g %g %g ",
			    vert[num_vert][X],
			    vert[num_vert][Y],
			    vert[num_vert][Z]);
		}
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RTRACE:
		fprintf(gOutfile, "5 %d %g 0 0 0 1 1 1 -\n",
			  gTexture_count, gTexture_ior);
		fprintf(gOutfile, "%d ", tot_vert);
		for (num_vert=0;num_vert<tot_vert;num_vert++)
		    fprintf(gOutfile, "%d ", num_vert+1);
		fprintf(gOutfile, "\n\n");
		for (num_vert=0;num_vert<tot_vert;num_vert++) {
		    fprintf(gOutfile, "%g %g %g\n",
			    vert[num_vert][X],
			    vert[num_vert][Y],
			    vert[num_vert][Z]);
		}
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_ART:
		tab_indent();
		fprintf(gOutfile, "polygon {\n");
		tab_inc();

		for (num_vert=0;num_vert<tot_vert;num_vert++) {
		    tab_indent();
		    fprintf(gOutfile, "vertex(%f, %f, %f)\n",
			      vert[num_vert][X],
			      vert[num_vert][Y],
			      vert[num_vert][Z]);
		}
		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RIB:
		tab_indent();
		fprintf(gOutfile, "Polygon \"P\" [\n");
		tab_inc();

		for (num_vert=tot_vert-1;num_vert>=0;num_vert--)
		{
		    tab_indent();
		    fprintf(gOutfile, "%#g %#g %#g\n",
			    vert[num_vert][X], vert[num_vert][Y],
			    vert[num_vert][Z]);
		}

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "]\n");
		break;

	 } /* switch */
    } /* else !OUTPUT_DELAYED */
}


/*-----------------------------------------------------------------*/
/*
 * Output polygonal patch.  A patch is defined by a set of vertices and their
 * normals.  With these databases, a patch is defined to have all points
 * coplanar.  A patch has only one side, with the order of the vertices being
 * counterclockwise as you face the patch (right-handed coordinate system).
 *
 * The output format is always:
 *   "pp" total_vertices
 *   vert1.x vert1.y vert1.z norm1.x norm1.y norm1.z
 *   [etc. for total_vertices polygonal patches]
 *
 */
void
lib_output_polypatch(tot_vert, vert, norm)
    int tot_vert;
    COORD3 vert[], norm[];
{
    /* None of the currently supported renderers are capable of directly
       generating polygon patches of more than 3 sides.   Therefore we
       will call a routine to split the patch into triangles.
    */
    split_polygon(tot_vert, vert, norm);
}
