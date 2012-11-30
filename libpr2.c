/*
 * libpr2.c - a library of primitive object output routines, part 2 of 3.
 *
 * Author:  Eric Haines, 3D/Eye, Inc.
 *
 */

/*-----------------------------------------------------------------*/
/* include section */
/*-----------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "lib.h"
#include "drv.h"


/*-----------------------------------------------------------------*/
/* defines/constants section */
/*-----------------------------------------------------------------*/


/*-----------------------------------------------------------------*/
/*
 * Output cylinder or cone.  A cylinder is defined as having a radius and an
 * axis defined by two points, which also define the top and bottom edge of the
 * cylinder.  A cone is defined similarly, the difference being that the apex
 * and base radii are different.  The apex radius is defined as being smaller
 * than the base radius.  Note that the surface exists without endcaps.
 *
 * If gRT_out_format=OUTPUT_CURVES, output the cylinder/cone in format:
 *     "c"
 *     base.x base.y base.z base_radius
 *     apex.x apex.y apex.z apex_radius
 *
 * If the format=OUTPUT_POLYGONS, the surface is polygonalized and output.
 * (4*OUTPUT_RESOLUTION) polygons are output as rectangles by
 * lib_output_polypatch.
 */
void
lib_output_cylcone(base_pt, apex_pt, curve_format)
    COORD4 base_pt, apex_pt;
    int curve_format;
{
    object_ptr new_object;
    COORD4  axis;
    double  len, cottheta, xang, yang, height;

    if (gRT_out_format == OUTPUT_DELAYED) {
	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	if (new_object == NULL)
	   /* Quietly fail */
	   return;
	new_object->object_type  = CONE_OBJ;
	new_object->curve_format = curve_format;
	new_object->surf_index   = gTexture_count;
	COPY_COORD4(new_object->object_data.cone.apex_pt, apex_pt);
	COPY_COORD4(new_object->object_data.cone.base_pt, base_pt);
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;

    } else if (curve_format == OUTPUT_CURVES) {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
	    case OUTPUT_PLG:
		lib_output_polygon_cylcone(base_pt, apex_pt);
		break;

	    case OUTPUT_NFF:
		fprintf(gOutfile, "c\n" ) ;
		fprintf(gOutfile, "%g %g %g %g\n",
			base_pt[X], base_pt[Y], base_pt[Z], base_pt[W]);
		fprintf(gOutfile, "%g %g %g %g\n",
			apex_pt[X], apex_pt[Y], apex_pt[Z], apex_pt[W]);
		break;

	    case OUTPUT_POVRAY_10:
		/*
		Since POV-Ray uses infinite primitives, we will start
		with a cone aligned with the z-axis (QCone_Z) and figure
		out how to clip and scale it to match what we want
		*/
		if (apex_pt[W] < base_pt[W]) {
		    /* Put the bigger end at the top */
		    COPY_COORD4(axis, base_pt);
		    COPY_COORD4(base_pt, apex_pt);
		    COPY_COORD4(apex_pt, axis);
		    }
		/* Find the axis and axis length */
		SUB3_COORD3(axis, apex_pt, base_pt);
		len = lib_normalize_vector(axis);
		if (len < EPSILON) {
		   /* Degenerate cone/cylinder */
		   fprintf(gOutfile, "// degenerate cone/cylinder!  Ignored...\n");
		   break;
		   }
		if (ABSOLUTE(apex_pt[W] - base_pt[W]) < EPSILON) {
		   /* Treat this thing as a cylinder */
		   cottheta = len;
		   tab_indent();
		   fprintf(gOutfile, "object {\n");
		   tab_inc();

		   tab_indent();
		   fprintf(gOutfile, "quadric { <1 1 0> <0 0 0> <0 0 0> -1 } // cylinder\n");

		   tab_indent();
		   fprintf(gOutfile, "clipped_by {\n");
		   tab_inc();

		   tab_indent();
		   fprintf(gOutfile, "intersection {\n");
		   tab_inc();

		   tab_indent();
		   fprintf(gOutfile, "plane { <0 0 -1> 0 }\n");
		   tab_indent();
		   fprintf(gOutfile, "plane { <0 0  1> 1 }\n");

		   tab_dec();
		   tab_indent();
		   fprintf(gOutfile, "} // intersection\n");

		   tab_dec();
		   tab_indent();
		   fprintf(gOutfile, "} // clip\n");

		   tab_indent();
		   fprintf(gOutfile, "scale <%g %g 1>\n", base_pt[W], base_pt[W]);
		   }
		else {
		   /* Determine alignment */
		   cottheta = len / (apex_pt[W] - base_pt[W]);
		   tab_indent();
		   fprintf(gOutfile, "object {\n");
		   tab_inc();

		   tab_indent();
		   fprintf(gOutfile, "quadric{ <1 1 -1> <0 0 0> <0 0 0> 0 } // cone\n");

		   tab_indent();
		   fprintf(gOutfile, "clipped_by {\n");
		   tab_inc();

		   tab_indent();
		   fprintf(gOutfile, "intersection {\n");
		   tab_inc();

		   tab_indent();
		   fprintf(gOutfile, "plane { <0 0 -1> %g}\n", -base_pt[W]);
		   tab_indent();
		   fprintf(gOutfile, "plane { <0 0  1> %g}\n", apex_pt[W]);

		   tab_dec();
		   tab_indent();
		   fprintf(gOutfile, "} // intersection\n");

		   tab_dec();
		   tab_indent();
		   fprintf(gOutfile, "} // clip\n");

		   tab_indent();
		   fprintf(gOutfile, "translate <0 0 %g>\n", -base_pt[W]);
		   }

		tab_indent();
		fprintf(gOutfile, "scale <1 1 %g>\n", cottheta);

		len = sqrt(axis[X] * axis[X] + axis[Z] * axis[Z]);
		xang = -180.0 * asin(axis[Y]) / PI;
		if (len < EPSILON)
		  yang = 0.0;
		else
		  yang = 180.0 * acos(axis[Z] / len) / PI;
		if (axis[X] < 0)
		   yang = -yang;
		tab_indent();
		fprintf(gOutfile, "rotate <%g %g 0>\n", xang, yang);
		tab_indent();
		fprintf(gOutfile, "translate <%g %g %g>\n",
			base_pt[X], base_pt[Y], base_pt[Z]);
		if (gTexture_name != NULL) {
		   tab_indent();
		   fprintf(gOutfile, "texture { %s }\n", gTexture_name);
		   }

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "} // object\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POVRAY_20:
		/* of course if apex_pt[W] ~= base_pt[W], could do cylinder */
		tab_indent();
		fprintf(gOutfile, "cone {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, "<%g, %g, %g>, %g,\n",
			apex_pt[X], apex_pt[Y], apex_pt[Z], apex_pt[W]);
		tab_indent();
		fprintf(gOutfile, "<%g, %g, %g>, %g open\n",
			base_pt[X], base_pt[Y], base_pt[Z], base_pt[W]);
		if (gTexture_name != NULL) {
		   tab_indent();
		   fprintf(gOutfile, "texture { %s }\n", gTexture_name);
		   }

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POLYRAY:
		tab_indent();
		fprintf(gOutfile, "object {");
		tab_inc();

		tab_indent();
		if (base_pt[W] == apex_pt[W])
		   fprintf(gOutfile, "cylinder <%g, %g, %g>, <%g, %g, %g>, %g",
			  base_pt[X], base_pt[Y], base_pt[Z],
			  apex_pt[X], apex_pt[Y], apex_pt[Z], apex_pt[W]);
		else
		   fprintf(gOutfile, "cone <%g, %g, %g>, %g, <%g, %g, %g>, %g",
			  base_pt[X], base_pt[Y], base_pt[Z], base_pt[W],
			  apex_pt[X], apex_pt[Y], apex_pt[Z], apex_pt[W]);
		if (gTexture_name != NULL)
		   fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, "\n");

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_VIVID:
		tab_indent();
		fprintf(gOutfile, "cone {");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, " base %g %g %g base_radius %g\n",
			base_pt[X], base_pt[Y], base_pt[Z], base_pt[W]);
		tab_indent();
		fprintf(gOutfile, " apex %g %g %g apex_radius %g\n",
			apex_pt[X], apex_pt[Y], apex_pt[Z], apex_pt[W]);

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_QRT:
		fprintf(gOutfile, "BEGIN_BBOX\n");
		lib_output_polygon_cylcone(base_pt, apex_pt);
		fprintf(gOutfile, "END_BBOX\n");
		break;

	    case OUTPUT_RAYSHADE:
		fprintf(gOutfile, "cone ");
		if (gTexture_name != NULL)
		    fprintf(gOutfile, "%s ", gTexture_name);
		fprintf(gOutfile, " %g %g %g %g %g %g %g %g\n",
			base_pt[W], base_pt[X], base_pt[Y], base_pt[Z],
			apex_pt[W], apex_pt[X], apex_pt[Y], apex_pt[Z]);
		break;

	    case OUTPUT_RTRACE:
		fprintf(gOutfile, "4 %d %g %g %g %g %g %g %g %g %g\n",
			gTexture_count, gTexture_ior,
			base_pt[X], base_pt[Y], base_pt[Z], base_pt[W],
			apex_pt[X], apex_pt[Y], apex_pt[Z], apex_pt[W]);
		break;

	    case OUTPUT_ART:
		if (base_pt[W] != apex_pt[W]) {
		    tab_indent();
		    fprintf(gOutfile, "cone {\n");
		    tab_inc();
		    tab_indent();
		    fprintf(gOutfile, "radius %g  center(%g, %g, %g)\n",
			    base_pt[W], base_pt[X], base_pt[Y], base_pt[Z]);
		    tab_indent();
		    fprintf(gOutfile, "radius %g  center(%g, %g, %g)\n",
			    apex_pt[W], apex_pt[X], apex_pt[Y], apex_pt[Z]);
		} else {
		    tab_indent();
		    fprintf(gOutfile, "cylinder {\n");
		    tab_inc();
		    tab_indent();
		    fprintf(gOutfile, "radius %g  center(%g, %g, %g)\n",
			    base_pt[W], base_pt[X], base_pt[Y], base_pt[Z]);
		    tab_indent();
		    fprintf(gOutfile, "center(%g, %g, %g)\n",
			    apex_pt[X], apex_pt[Y], apex_pt[Z]);
		}

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RAWTRI:
	    case OUTPUT_DXF:
		lib_output_polygon_cylcone(base_pt, apex_pt);
		break;

	    case OUTPUT_RIB:
		/* translate and orient */
		tab_indent();
		fprintf(gOutfile, "TransformBegin\n");
		tab_inc();

		SUB3_COORD3(axis, apex_pt, base_pt);
		height= len = lib_normalize_vector(axis);
		if (len < EPSILON)
		{
		  /* Degenerate cone/cylinder */
		  fprintf(gOutfile, "# degenerate cone/cylinder!\n"
			  "Ignored...\n");
		  break;
		}

		axis_to_z(axis, &xang, &yang);

		/* Calculate transformation from intrisic position */
		tab_indent();
		fprintf(gOutfile, "Translate %#g %#g %#g\n",
			base_pt[X], base_pt[Y], base_pt[Z]);
		tab_indent();
		fprintf(gOutfile, "Rotate %#g 0 1 0\n", yang);  /* was -yang */
		tab_indent();
		fprintf(gOutfile, "Rotate %#g 1 0 0\n", xang);  /* was -xang */
		if (ABSOLUTE(apex_pt[W] - base_pt[W]) < EPSILON)
		{
		  /* Treat this thing as a cylinder */
		  tab_indent();
		  fprintf(gOutfile, "Cylinder [ %#g %#g %#g %#g ]\n",
			  apex_pt[W], 0.0, len, 360.0);
		}
		else
		{
		  /* We use a hyperboloid, because a cone cannot be cut
		   * at the top */
		  tab_indent();
		  fprintf(gOutfile, "Hyperboloid %#g 0 0  %#g 0 %#g  360.0\n",
			  base_pt[W], apex_pt[W], height);
		}

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "TransformEnd\n");
		break;
	      }
    }
    else
      lib_output_polygon_cylcone(base_pt, apex_pt);
}


/*-----------------------------------------------------------------*/
void
lib_output_disc(center, normal, iradius, oradius, curve_format)
    COORD3 center, normal;
    double iradius, oradius;
    int curve_format;
{
    object_ptr new_object;
    COORD4  axis, base, apex;
    COORD3  axis_rib ;
    double  len, xang, yang;

    if (gRT_out_format == OUTPUT_DELAYED) {
	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	if (new_object == NULL)
	    /* Quietly fail */
	    return;
	new_object->object_type  = DISC_OBJ;
	new_object->curve_format = curve_format;
	new_object->surf_index   = gTexture_count;
	COPY_COORD4(new_object->object_data.disc.center, center);
	COPY_COORD4(new_object->object_data.disc.normal, normal);
	new_object->object_data.disc.iradius = iradius;
	new_object->object_data.disc.iradius = oradius;
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;
    } else if (curve_format == OUTPUT_CURVES) {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
	    case OUTPUT_NFF:
	    case OUTPUT_PLG:
	    case OUTPUT_VIVID:
	    case OUTPUT_RAYSHADE:
	    case OUTPUT_RAWTRI:
	    case OUTPUT_DXF:
		lib_output_polygon_disc(center, normal, iradius, oradius);
		break;

	    case OUTPUT_POVRAY_10:
		/* A disc is a plane intersected with either one or two
		 * spheres
		 */
		COPY_COORD3(axis, normal);
		len = lib_normalize_vector(axis);
		tab_indent();
		fprintf(gOutfile, "object {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, "plane { <0 0 1> 1 }\n");

		tab_indent();
		fprintf(gOutfile, "clipped_by {\n");
		tab_inc();

		if (iradius > 0.0) {
		    tab_indent();
		    fprintf(gOutfile, "intersection {\n");
		    tab_inc();

		    tab_indent();
		    fprintf(gOutfile, "sphere { <0 0 0> %g inverse }\n",
			    iradius);
		    tab_indent();
		    fprintf(gOutfile, "sphere { <0 0 1> %g }\n", oradius);

		    tab_dec();
		    tab_indent();
		    fprintf(gOutfile, "} // intersection\n");
		}
		else {
		    tab_indent();
		    fprintf(gOutfile, "object { sphere { <0 0 0> %g } }\n",
			    oradius);
		}

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "} // clip\n");

		len = sqrt(axis[X] * axis[X] + axis[Z] * axis[Z]);
		xang = -180.0 * asin(axis[Y]) / PI;
		yang = 180.0 * acos(axis[Z] / len) / PI;
		if (axis[X] < 0)
		    yang = -yang;
		tab_indent();
		fprintf(gOutfile, "rotate <%g %g 0>\n", xang, yang);
		tab_indent();
		fprintf(gOutfile, "translate <%g %g %g>\n",
			center[X], center[Y], center[Z]);

		if (gTexture_name != NULL) {
		    tab_indent();
		    fprintf(gOutfile, "texture { %s }", gTexture_name);
		}

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "} // object - disc\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POVRAY_20:
		/* disc <center> <normalVector> radius [holeRadius] */
		tab_indent();
		fprintf(gOutfile, "disc { <%g, %g, %g>",
			center[X], center[Y], center[Z]);
		fprintf(gOutfile, " <%g, %g, %g>",
			normal[X], normal[Y], normal[Z]);
		fprintf(gOutfile, " %g", oradius);
		if (iradius > 0.0)
		    fprintf(gOutfile, ", %g", iradius);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " texture { %s }", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POLYRAY:
		tab_indent();
		fprintf(gOutfile, "object { disc <%g, %g, %g>,",
			center[X], center[Y], center[Z]);
		fprintf(gOutfile, " <%g, %g, %g>,",
			normal[X], normal[Y], normal[Z]);
		if (iradius > 0.0)
		    fprintf(gOutfile, " %g,", iradius);
		fprintf(gOutfile, " %g", oradius);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_QRT:
		fprintf(gOutfile, "BEGIN_BBOX\n");
		lib_output_polygon_disc(center, normal, iradius, oradius);
		fprintf(gOutfile, "END_BBOX\n");
		break;

	    case OUTPUT_RTRACE:
		COPY_COORD3(base, center);
		base[W] = iradius;
		apex[X] = center[X] + normal[X] * EPSILON2;
		apex[Y] = center[Y] + normal[Y] * EPSILON2;
		apex[Z] = center[Z] + normal[Z] * EPSILON2;
		apex[W] = oradius;
		lib_output_cylcone(base, apex, curve_format);
		break;
	      case OUTPUT_RIB:
		if (iradius > 0)
		{
		  /* translate and orient */
		  tab_indent();
		  fprintf(gOutfile, "TransformBegin\n");
		  tab_inc();

		  /* Calculate transformation from intrisic position */
		  COPY_COORD3(axis_rib, normal);
		  len = lib_normalize_vector(axis_rib);
		  axis_to_z(axis_rib, &xang, &yang);

		  tab_indent();
		  fprintf(gOutfile, "translate %#g %#g %#g\n",
			  center[X], center[Y], center[Z]);
		  tab_indent();
		  fprintf(gOutfile, "Rotate %#g 0 1 0\n", yang);  /* was -yang */
		  tab_indent();
		  fprintf(gOutfile, "Rotate %#g 1 0 0\n", xang);  /* was -xang */
		  tab_indent();
		  fprintf(gOutfile, "Disk 0 %#g 360\n", oradius);
		  tab_dec();
		  fprintf(gOutfile, "TransformEnd\n");
		}
		else
		  lib_output_polygon_disc(center, normal, iradius, oradius);
		break;
	      }
    } else {
	lib_output_polygon_disc(center, normal, iradius, oradius);
    }
}


/*-----------------------------------------------------------------*/
static void
sq_sphere_val(a1, a2, a3, n, e, u, v, P)
    double a1, a2, a3, n, e, u, v;
    COORD3 P;
{
    double cu, su, cv, sv;
    double icu, isu, icv, isv;

    cu = cos(u); su = sin(u);
    cv = cos(v); sv = sin(v);
    icu = SGN(cu); isu = SGN(su);
    icv = SGN(cv); isv = SGN(sv);
    cu = fabs(cu); cv = fabs(cv);
    su = fabs(su); sv = fabs(sv);
    P[X] = a1 * POW(cv, n) * POW(cu, e) * icv * icu;
    P[Y] = a2 * POW(cv, n) * POW(su, e) * icv * isu;
    P[Z] = a3 * POW(sv, n) * isv;
}

/*-----------------------------------------------------------------*/
static void
sq_sphere_norm(a1, a2, a3, n, e, u, v, N)
    double a1, a2, a3, n, e, u, v;
    COORD3 N;
{
    double cu, su, cv, sv;
    double icu, isu, icv, isv;

    cu = cos(u); su = sin(u);
    cv = cos(v); sv = sin(v);
    icu = SGN(cu); isu = SGN(su);
    icv = SGN(cv); isv = SGN(sv);

    /* May be some singularities in the values, lets catch them & put
      a fudged normal into N */
    if (e < 2 || n < 2) {
	if (ABSOLUTE(cu) < 1.0e-3 || ABSOLUTE(su) < 1.0e-3 ||
	    ABSOLUTE(cu) < 1.0e-3 || ABSOLUTE(su) < 1.0e-3) {
	   SET_COORD3(N, cu*cv, su*cv, sv);
	   lib_normalize_vector(N);
	   return;
	}
    }

    cu = fabs(cu); cv = fabs(cv);
    su = fabs(su); sv = fabs(sv);

    N[X] = a1 * POW(cv, 2-n) * POW(cu, 2-e) * icv * icu;
    N[Y] = a2 * POW(cv, 2-n) * POW(su, 2-e) * icv * isu;
    N[Z] = a3 * POW(sv, 2-n) * isv;
    lib_normalize_vector(N);
}

/*-----------------------------------------------------------------*/
void
lib_output_sq_sphere(center_pt, a1, a2, a3, n, e)
    COORD3 center_pt;
    double a1, a2, a3, n, e;
{
    object_ptr new_object;
    int i, j, u_res, v_res;
    double u, delta_u, v, delta_v;
    COORD3 verts[4], norms[4];

    if (gRT_out_format == OUTPUT_DELAYED) {
       /* Save all the pertinent information */
       new_object = (object_ptr)malloc(sizeof(struct object_struct));
       if (new_object == NULL)
	   /* Quietly fail */
	   return;
       new_object->object_type  = SUPERQ_OBJ;
       new_object->curve_format = OUTPUT_PATCHES;
       new_object->surf_index   = gTexture_count;
       COPY_COORD4(new_object->object_data.superq.center_pt, center_pt);
       new_object->object_data.superq.a1 = a1;
       new_object->object_data.superq.a2 = a2;
       new_object->object_data.superq.a3 = a3;
       new_object->object_data.superq.n  = n;
       new_object->object_data.superq.e  = e;
       new_object->next_object = gLib_objects;
       gLib_objects = new_object;
       return;
    }

    u_res = 4 * gU_resolution;
    v_res = 4 * gV_resolution;
    delta_u = 2.0 * PI / (double)u_res;
    delta_v = PI / (double)v_res;

    for (i=0,u=0.0;i<u_res;i++,u+=delta_u) {
	PLATFORM_MULTITASK();
	for (j=0,v=-PI/2.0;j<v_res;j++,v+=delta_v) {
	    if (j == 0) {
		sq_sphere_val(a1, a2, a3, n, e, u, v, verts[0]);
		sq_sphere_norm(a1, a2, a3, n, e, u, v, norms[0]);
		sq_sphere_val(a1, a2, a3, n, e, u, v+delta_v, verts[1]);
		sq_sphere_norm(a1, a2, a3, n, e, u, v+delta_v, norms[1]);
		sq_sphere_val(a1, a2, a3, n, e, u+delta_u, v+delta_v, verts[2]);
		sq_sphere_norm(a1, a2, a3, n, e, u+delta_u, v+delta_v,norms[2]);
		ADD3_COORD3(verts[0], verts[0], center_pt);
		ADD3_COORD3(verts[1], verts[1], center_pt);
		ADD3_COORD3(verts[2], verts[2], center_pt);
		lib_output_polypatch(3, verts, norms);
	    } else if (j == v_res-1) {
		sq_sphere_val(a1, a2, a3, n, e, u, v, verts[0]);
		sq_sphere_norm(a1, a2, a3, n, e, u, v, norms[0]);
		sq_sphere_val(a1, a2, a3, n, e, u, v+delta_v, verts[1]);
		sq_sphere_norm(a1, a2, a3, n, e, u, v+delta_v, norms[1]);
		sq_sphere_val(a1, a2, a3, n, e, u+delta_u, v, verts[2]);
		sq_sphere_norm(a1, a2, a3, n, e, u+delta_u, v, norms[2]);
		ADD3_COORD3(verts[0], verts[0], center_pt);
		ADD3_COORD3(verts[1], verts[1], center_pt);
		ADD3_COORD3(verts[2], verts[2], center_pt);
		lib_output_polypatch(3, verts, norms);
	    } else {
		sq_sphere_val(a1, a2, a3, n, e, u, v, verts[0]);
		sq_sphere_norm(a1, a2, a3, n, e, u, v, norms[0]);
		sq_sphere_val(a1, a2, a3, n, e, u, v+delta_v, verts[1]);
		sq_sphere_norm(a1, a2, a3, n, e, u, v+delta_v, norms[1]);
		sq_sphere_val(a1, a2, a3, n, e, u+delta_u, v+delta_v, verts[2]);
		sq_sphere_norm(a1, a2, a3, n, e, u+delta_u, v+delta_v,norms[2]);
		ADD3_COORD3(verts[0], verts[0], center_pt);
		ADD3_COORD3(verts[1], verts[1], center_pt);
		ADD3_COORD3(verts[2], verts[2], center_pt);
		lib_output_polypatch(3, verts, norms);
		COPY_COORD3(verts[1], verts[2]);
		COPY_COORD3(norms[1], norms[2]);
		sq_sphere_val(a1, a2, a3, n, e, u+delta_u, v, verts[2]);
		sq_sphere_norm(a1, a2, a3, n, e, u+delta_u, v, norms[2]);
		ADD3_COORD3(verts[2], verts[2], center_pt);
		lib_output_polypatch(3, verts, norms);
	    }
	}
    }
}


/*-----------------------------------------------------------------*/
/*
 * Output sphere.  A sphere is defined by a radius and center position.
 *
 * If format=OUTPUT_CURVES, output the sphere in format:
 *     "s" center.x center.y center.z radius
 *
 * If the format=OUTPUT_POLYGONS, the sphere is polygonalized and output.
 * The sphere is polygonalized by splitting it into 6 faces (of a cube
 * projected onto the sphere) and dividing these faces by equally spaced
 * great circles.  OUTPUT_RESOLUTION affects the number of great circles.
 * (6*2*gU_resolution*gV_resolution) polygons are output as triangles
 * using lib_output_polypatch.
 */
void
lib_output_sphere(center_pt, curve_format)
    COORD4 center_pt;
    int curve_format;
{
    object_ptr new_object;

    if (gRT_out_format == OUTPUT_DELAYED) {
	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	if (new_object == NULL)
	    /* Quietly fail */
	    return;
	new_object->object_type  = SPHERE_OBJ;
	new_object->curve_format = curve_format;
	new_object->surf_index   = gTexture_count;
	COPY_COORD4(new_object->object_data.sphere.center_pt, center_pt);
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;
    }
    else if (curve_format == OUTPUT_CURVES) {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
	    case OUTPUT_PLG:
		lib_output_polygon_sphere(center_pt);
		break;

	    case OUTPUT_NFF:
		fprintf(gOutfile, "s %g %g %g %g\n",
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		break;

	    case OUTPUT_POVRAY_10:
		tab_indent();
		fprintf(gOutfile, "object { sphere { <%g %g %g> %g }",
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " texture { %s }", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POVRAY_20:
		tab_indent();
		fprintf(gOutfile, "sphere { <%g, %g, %g>, %g",
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " texture { %s }", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POLYRAY:
		tab_indent();
		fprintf(gOutfile, "object { sphere <%g, %g, %g>, %g",
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_VIVID:
		tab_indent();
		fprintf(gOutfile, "sphere { center %g %g %g radius %g }\n",
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_QRT:
		tab_indent();
		fprintf(gOutfile, "sphere ( loc = (%g, %g, %g), radius = %g )\n",
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		break;

	    case OUTPUT_RAYSHADE:
		fprintf(gOutfile, "sphere ");
		if (gTexture_name != NULL)
		    fprintf(gOutfile, "%s ", gTexture_name);
		fprintf(gOutfile, " %g %g %g %g\n",
			center_pt[W], center_pt[X], center_pt[Y], center_pt[Z]);
		break;

	    case OUTPUT_RTRACE:
		fprintf(gOutfile, "1 %d %g %g %g %g %g\n",
			gTexture_count, gTexture_ior,
			center_pt[X], center_pt[Y], center_pt[Z], center_pt[W]);
		break;

	    case OUTPUT_ART:
		tab_indent();
		fprintf(gOutfile, "sphere {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, "radius %g\n", center_pt[W]);
		tab_indent();
		fprintf(gOutfile, "center(%g, %g, %g)\n",
			center_pt[X], center_pt[Y], center_pt[Z]);

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RAWTRI:
	    case OUTPUT_DXF:
		lib_output_polygon_sphere(center_pt);
		break;
	      case OUTPUT_RIB:
		tab_indent();
		fprintf(gOutfile, "TransformBegin\n");
		tab_inc();
		tab_indent();
		fprintf(gOutfile, "Translate %#g %#g %#g\n",
			center_pt[X], center_pt[Y], center_pt[Z]);
		tab_indent();
		fprintf(gOutfile, "Sphere %#g %#g %#g 360\n",
			center_pt[W], -center_pt[W], center_pt[W]);
		tab_dec();
		tab_indent();
		fprintf(gOutfile, "TransformEnd\n");
		break;
	}
    } else {
	lib_output_polygon_sphere(center_pt);
    }
}



/*-----------------------------------------------------------------*/
/* Output box.  A box is defined by a diagonally opposite corners. */
void
lib_output_box(p1, p2)
    COORD3 p1, p2;
{
    object_ptr new_object;

    if (gRT_out_format == OUTPUT_DELAYED) {
	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	if (new_object == NULL)
	    /* Quietly fail */
	    return;
	new_object->object_type  = BOX_OBJ;
	new_object->curve_format = OUTPUT_PATCHES;
	new_object->surf_index   = gTexture_count;
	COPY_COORD3(new_object->object_data.box.point1, p1);
	COPY_COORD3(new_object->object_data.box.point2, p2);
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;
    } else {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
	    case OUTPUT_NFF:
	    case OUTPUT_VIVID:
	    case OUTPUT_PLG:
	    case OUTPUT_RAWTRI:
	    case OUTPUT_RIB:
	    case OUTPUT_DXF:
		lib_output_polygon_box(p1, p2);
		break;

	    case OUTPUT_POVRAY_10:
		tab_indent();
		fprintf(gOutfile, "object { box { <%g %g %g> <%g %g %g> }",
			p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " texture { %s }", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POVRAY_20:
		tab_indent();
		fprintf(gOutfile, "box { <%g, %g, %g>, <%g, %g, %g>  ",
			p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " texture { %s }", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POLYRAY:
		fprintf(gOutfile, "object { box <%g, %g, %g>, <%g, %g, %g>",
			p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_QRT:
		fprintf(gOutfile, "BEGIN_BBOX\n");
		lib_output_polygon_box(p1, p2);
		fprintf(gOutfile, "END_BBOX\n");
		break;

	    case OUTPUT_RAYSHADE:
		fprintf(gOutfile, "box ");
		if (gTexture_name != NULL)
		    fprintf(gOutfile, "%s ", gTexture_name);
		fprintf(gOutfile, " %g %g %g %g %g %g\n",
			p1[X], p1[Y], p1[Z], p2[X], p2[Y], p2[Z]);
		break;

	    case OUTPUT_ART:
		tab_indent();
		fprintf(gOutfile, "box {");
		fprintf(gOutfile, " vertex(%g, %g, %g)\n",
			p1[X], p1[Y], p1[Z]);
		fprintf(gOutfile, " vertex(%g, %g, %g) }\n",
			p2[X], p2[Y], p2[Z]);
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RTRACE:
		fprintf(gOutfile, "2 %d %g %g %g %g %g %g %g\n",
			gTexture_count, gTexture_ior,
			(p1[X] + p2[X]) / 2.0,
			(p1[Y] + p2[Y]) / 2.0,
			(p1[Z] + p2[Z]) / 2.0,
			p2[X] - p1[X], p2[Y] - p1[Y], p2[Z] - p1[Z]);
		break;
	}
    }
}
