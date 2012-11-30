/*
 * libpr1.c - a library of primitive object output routines, part 1 of 3.
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
void lib_output_comment (comment)
	char *comment;
{
    switch (gRT_out_format) {

	case OUTPUT_VIDEO:
	case OUTPUT_DELAYED:
	case OUTPUT_RAWTRI:
	case OUTPUT_DXF:        /* well, there's the 999 format, but... >>>>> */
	case OUTPUT_RWX:
		/* no comments allowed for these file formats */
	    break;

	case OUTPUT_NFF:
	case OUTPUT_OBJ:
	case OUTPUT_RIB:
		fprintf(gOutfile, "# %s\n", comment);
	    break;

	case OUTPUT_RAYSHADE:
	case OUTPUT_POLYRAY:
	case OUTPUT_POVRAY_10:
	case OUTPUT_POVRAY_20:
		fprintf(gOutfile, "// %s\n", comment);
	    break;

	/* unknown comment formats... whoever knows, please fix or fill in! */
	case OUTPUT_PLG:
	case OUTPUT_VIVID:
	case OUTPUT_RTRACE:
	case OUTPUT_QRT:
	case OUTPUT_ART:
	default:
		fprintf(gOutfile, "// <comment> '%s'\n", comment);
	    break;

	}
} /* lib_output_comment */


/*-----------------------------------------------------------------*/
void lib_output_vector(x, y, z)
double  x, y, z;
{
    switch (gRT_out_format) {
	case OUTPUT_VIDEO:
	case OUTPUT_DELAYED:
	case OUTPUT_DXF:
	case OUTPUT_RWX:
	    break;

	case OUTPUT_PLG:
	case OUTPUT_OBJ:
	case OUTPUT_NFF:
	case OUTPUT_VIVID:
	case OUTPUT_RAYSHADE:
	case OUTPUT_RTRACE:
	case OUTPUT_RAWTRI:
		fprintf(gOutfile, "%g %g %g", x, y, z);
	    break;

	case OUTPUT_POVRAY_10:
		fprintf(gOutfile, "<%g %g %g>", x, y, z);
	    break;
	case OUTPUT_POVRAY_20:
		fprintf(gOutfile, "<%g, %g, %g>", x, y, z);
	    break;

	case OUTPUT_POLYRAY:
	case OUTPUT_QRT:
	case OUTPUT_ART:
		fprintf(gOutfile, "%g, %g, %g", x, y, z);
	    break;
	}
} /* lib_output_vector */


/*-----------------------------------------------------------------*/
void
axis_to_z(axis, xang, yang)
     COORD3 axis;
     double *xang, *yang;
{
  double len;

  /* The axis is in RH coordinates and turns the axis to Z axis */
  /* The angles are for the LH coordinate system */
  len = sqrt(axis[X] * axis[X] + axis[Z] * axis[Z]);
  *xang = -180.0 * asin(axis[Y]) / PI;
  if (len < EPSILON)
    *yang = 0.0;
  else
    *yang = 180.0 * acos(axis[Z] / len) / PI;
  if (axis[X] < 0)
    *yang = -(*yang);
} /* axis_to_z */

/*-----------------------------------------------------------------*/
/*
 * Output viewpoint location.  The parameters are:
 *   From:   The eye location.
 *   At:     A position to be at the center of the image.  A.k.a. "lookat"
 *   Up:     A vector defining which direction is up.
 *   Fov:    Vertical field of view of the camera
 *   Aspect: Aspect ratio of horizontal fov to vertical fov
 *   Hither: Minimum distance to any ray-surface intersection
 *   Resx:   X resolution of resulting image
 *   Resy:   Y resolution of resulting image
 *
 * For all databases some viewing parameters are always the same:
 *
 *   Viewing angle is defined as from the center of top pixel row to bottom
 *     pixel row and left column to right column.
 *   Yon is "at infinity."
 */
void
lib_output_viewpoint(from, at, up,
		     fov_angle, aspect_ratio, hither,
		     resx, resy)
    COORD3 from, at, up;
    double fov_angle, aspect_ratio, hither;
    int    resx, resy;
{
    COORD3 axis, myright, tmp;
    COORD4 viewvec, rightvec;
    MATRIX m1;
    double tmpf;
    double frustrumheight, frustrumwidth;

    switch (gRT_out_format) {
	case OUTPUT_DELAYED:
	case OUTPUT_VIDEO:
	case OUTPUT_PLG:
	case OUTPUT_OBJ:
	case OUTPUT_RWX:
	    /* Save the various view parameters */
	    COPY_COORD3(gViewpoint.from, from);
	    COPY_COORD3(gViewpoint.at, at);
	    COPY_COORD3(gViewpoint.up, up);
	    gViewpoint.angle  = fov_angle;
	    gViewpoint.hither = hither;
	    gViewpoint.resx   = resx;
	    gViewpoint.resy   = resy;
	    gViewpoint.aspect = aspect_ratio;

	    /* Make the 3D clipping box for this view */
	    gView_bounds[0][0] = 0;
	    gView_bounds[1][0] = gViewpoint.resx;
	    gView_bounds[0][1] = 0;
	    gView_bounds[1][1] = gViewpoint.resy;
	    gView_bounds[0][2] = gViewpoint.hither;
	    gView_bounds[1][2] = 1.0e10;

	    /* Generate the perspective view matrix */
	    lib_create_view_matrix(gViewpoint.tx, gViewpoint.from, gViewpoint.at,
				   gViewpoint.up, gViewpoint.resx, gViewpoint.resy,
				   gViewpoint.angle, gViewpoint.aspect);

	    /* Turn on graphics using system dependent video routines */
	    if (gRT_out_format == OUTPUT_VIDEO) {
		display_init(gViewpoint.resx, gViewpoint.resy, gBkgnd_color);
		gView_init_flag = 1;
	    }
	    break;

	case OUTPUT_NFF:
	    fprintf(gOutfile, "v\n");
	    fprintf(gOutfile, "from %g %g %g\n", from[X], from[Y], from[Z]);
	    fprintf(gOutfile, "at %g %g %g\n", at[X], at[Y], at[Z]);
	    fprintf(gOutfile, "up %g %g %g\n", up[X], up[Y], up[Z]);
	    fprintf(gOutfile, "angle %g\n", fov_angle);
	    fprintf(gOutfile, "hither %g\n", hither);
	    fprintf(gOutfile, "resolution %d %d\n", resx, resy);
	    break;

	case OUTPUT_POVRAY_10:
	case OUTPUT_POVRAY_20:
	    /* Let's get a set of vectors that are all at right angles to each
	       other that describe the view given. */
	    lib_normalize_vector(up);
	    SUB3_COORD3(viewvec, at, from);
	    lib_normalize_vector(viewvec);
	    CROSS(rightvec, up, viewvec);
	    lib_normalize_vector(rightvec);
	    CROSS(up, viewvec, rightvec);
	    lib_normalize_vector(up);

	    /* Calculate the height of the view frustrum in world coordinates.
	       and then scale the right and up vectors appropriately. */
	    frustrumheight = 2.0 * tan(PI * fov_angle / 360.0);
	    frustrumwidth = aspect_ratio * frustrumheight;
	    up[X] *= frustrumheight;
	    up[Y] *= frustrumheight;
	    up[Z] *= frustrumheight;
	    rightvec[X] *= frustrumwidth;
	    rightvec[Y] *= frustrumwidth;
	    rightvec[Z] *= frustrumwidth;

	    tab_indent();
	    fprintf(gOutfile, "camera {\n");
	    tab_inc();

	    if (gRT_out_format == OUTPUT_POVRAY_10) {
		tab_indent();
		fprintf(gOutfile, "location <%g %g %g>\n",
			from[X], from[Y], from[Z]);
		tab_indent();
		fprintf(gOutfile, "direction <%g %g %g>\n",
			viewvec[X], viewvec[Y], viewvec[Z]);
		tab_indent();
		fprintf(gOutfile, "right <%g %g %g>\n",
			-rightvec[X], -rightvec[Y], -rightvec[Z]);
		tab_indent();
		fprintf(gOutfile, "up <%g %g %g>\n",
			up[X], up[Y], up[Z]);
		}
	    else {
		tab_indent();
		fprintf(gOutfile, "location <%g, %g, %g>\n",
			from[X], from[Y], from[Z]);
		tab_indent();
		fprintf(gOutfile, "direction <%g, %g, %g>\n",
			viewvec[X], viewvec[Y], viewvec[Z]);
		tab_indent();
		fprintf(gOutfile, "right <%g, %g, %g>\n",
			-rightvec[X], -rightvec[Y], -rightvec[Z]);
		tab_indent();
		fprintf(gOutfile, "up <%g, %g, %g>\n",
			up[X], up[Y], up[Z]);
	    }
	    tab_dec();
	    fprintf(gOutfile, "} // camera\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POLYRAY:
	    tab_indent();
	    fprintf(gOutfile, "viewpoint {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "from <%g, %g, %g>\n", from[X], from[Y], from[Z]);
	    tab_indent();
	    fprintf(gOutfile, "at <%g, %g, %g>\n", at[X], at[Y], at[Z]);
	    tab_indent();
	    fprintf(gOutfile, "up <%g, %g, %g>\n", up[X], up[Y], up[Z]);
	    tab_indent();
	    fprintf(gOutfile, "angle %g\n", fov_angle);
	    tab_indent();
	    /* Note the negative, this is to change to right handed
	       coordinates (like most of the other tracers) */
	    fprintf(gOutfile, "aspect %g\n", -aspect_ratio);
	    tab_indent();
	    fprintf(gOutfile, "hither %g\n", hither);
	    tab_indent();
	    fprintf(gOutfile, "resolution %d, %d\n", resx, resy);

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "}\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_VIVID:
	    tab_indent();
	    fprintf(gOutfile, "studio {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "from %g %g %g\n", from[X], from[Y], from[Z]);
	    tab_indent();
	    fprintf(gOutfile, "at %g %g %g\n", at[X], at[Y], at[Z]);
	    tab_indent();
	    fprintf(gOutfile, "up %g %g %g\n", up[X], up[Y], up[Z]);
	    tab_indent();
	    fprintf(gOutfile, "angle %g\n", fov_angle);
	    tab_indent();
	    fprintf(gOutfile, "aspect %g\n", aspect_ratio);
	    tab_indent();
	    fprintf(gOutfile, "resolution %d %d\n", resx, resy);
	    tab_indent();
	    fprintf(gOutfile, "no_exp_trans\n");

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "}\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_QRT:
	    tab_indent();
	    fprintf(gOutfile, "OBSERVER = (\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "loc = (%g,%g,%g),\n", from[X], from[Y], from[Z]);
	    tab_indent();
	    fprintf(gOutfile, "lookat = (%g,%g,%g),\n", at[X], at[Y], at[Z]);
	    tab_indent();
	    fprintf(gOutfile, "up = (%g,%g,%g)\n", up[X], up[Y], up[Z]);
	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, ")\n");

	    tab_indent();
	    fprintf(gOutfile, "FOC_LENGTH = %g\n",
		    35.0 / tan(PI * fov_angle / 360.0));
	    tab_indent();
	    fprintf(gOutfile, "DEFAULT (\n");
	    tab_inc();
	    tab_indent();
	    fprintf(gOutfile, "aspect = %g,\n", 6.0 * aspect_ratio / 7.0);
	    tab_indent();
	    fprintf(gOutfile, "x_res = %d, y_res = %d\n", resx, resy);
	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, ")\n");

	    /* QRT insists on having the output file as part of the data text */
	    tab_indent();
	    fprintf(gOutfile, "FILE_NAME = qrt.tga\n");
	    break;

	case OUTPUT_RAYSHADE:
	    fprintf(gOutfile, "eyep %g %g %g\n", from[X], from[Y], from[Z]);
	    fprintf(gOutfile, "lookp %g %g %g\n", at[X], at[Y], at[Z]);
	    fprintf(gOutfile, "up %g %g %g\n", up[X], up[Y], up[Z]);
	    fprintf(gOutfile, "fov %g %g\n", aspect_ratio * fov_angle,
		     fov_angle);
	    fprintf(gOutfile, "screen %d %d\n", resx, resy);
	    fprintf(gOutfile, "sample 1 nojitter\n");
	    break;

	case OUTPUT_RTRACE:
	    fprintf(gOutfile, "View\n");
	    fprintf(gOutfile, "%g %g %g\n", from[X], from[Y], from[Z]);
	    fprintf(gOutfile, "%g %g %g\n", at[X], at[Y], at[Z]);
	    fprintf(gOutfile, "%g %g %g\n", up[X], up[Y], up[Z]);
	    fprintf(gOutfile, "%g %g\n", aspect_ratio * fov_angle/2,
		     fov_angle/2);
	    break;

	case OUTPUT_ART:
	    fprintf(gOutfile, "maxhitlevel 4\n");
	    fprintf(gOutfile, "screensize 0.0, 0.0\n");
	    fprintf(gOutfile, "fieldofview %g\n", fov_angle);
	    fprintf(gOutfile, "up(%g, %g, %g)\n", up[X], up[Y], up[Z]);
	    fprintf(gOutfile, "lookat(%g, %g, %g, ", from[X], from[Y], from[Z]);
	    fprintf(gOutfile, "%g, %g, %g, 0.0)\n", at[X], at[Y], at[Z]);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_RAWTRI:
	    break;

	case OUTPUT_RIB:
	    fprintf(gOutfile, "version 3.03\n");
	    fprintf(gOutfile, "FrameBegin 1\n");
	    fprintf(gOutfile, "Format %d %d 1\n", resx, resy);
	    fprintf(gOutfile, "PixelSamples 1 1\n");
	    fprintf(gOutfile, "ShadingRate 1.0\n");
	    fprintf(gOutfile, "Projection \"perspective\" \"fov\" %#g\n",
		    fov_angle);
	    fprintf(gOutfile, "Clipping %#g %#g\n\n", hither, 1e38);

	    /* Calculate transformation from intrisic position */
	    SUB3_COORD3(axis, at, from);
	    lib_normalize_vector(axis);

	    COPY_COORD3(tmp,axis);
	    tmpf = DOT_PRODUCT(up,axis);
	    tmp[0] *= tmpf;  tmp[1] *= tmpf;  tmp[2] *= tmpf;
	    SUB2_COORD3(up,tmp);
	    lib_normalize_vector(up);

	    CROSS(myright,up,axis);
	    lib_normalize_vector (myright);

	    m1[0][0] = myright[0];  m1[1][0] = myright[1];
	    m1[2][0] = myright[2];  m1[3][0] = 0;
	    m1[0][1] = up[0];  m1[1][1] = up[1];
	    m1[2][1] = up[2]; m1[3][1] = 0;
	    m1[0][2] = axis[0];  m1[1][2] = axis[1];
	    m1[2][2] = axis[2];  m1[3][2] = 0;
	    m1[0][3] = 0;  m1[1][3] = 0;  m1[2][3] = 0;  m1[3][3] = 1;
	    fprintf (gOutfile, "ConcatTransform [%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g]\n",
		     m1[0][0], m1[0][1], m1[0][2], m1[0][3],
		     m1[1][0], m1[1][1], m1[1][2], m1[1][3],
		     m1[2][0], m1[2][1], m1[2][2], m1[2][3],
		     m1[3][0], m1[3][1], m1[3][2], m1[3][3]);
	    fprintf (gOutfile, "Translate %g %g %g\n", -from[0], -from[1], -from[2]);

	    fprintf(gOutfile, "WorldBegin\n");
	    tab_inc();
	    break ;

	case OUTPUT_DXF:
	    fprintf(gOutfile, "  0\n" ) ;
	    fprintf(gOutfile, "SECTION\n" ) ;
	    fprintf(gOutfile, "  2\n" ) ;
	    fprintf(gOutfile, "HEADER\n" ) ;
	    fprintf(gOutfile, "  0\n" ) ;
	    fprintf(gOutfile, "ENDSEC\n" ) ;
	    fprintf(gOutfile, "  0\n" ) ;
	    fprintf(gOutfile, "SECTION\n" ) ;
	    fprintf(gOutfile, "  2\n" ) ;
	    fprintf(gOutfile, "ENTITIES\n" ) ;
	    /* should add view someday ... */
	    break;
    }
}

/*-----------------------------------------------------------------*/
/*
 * Output light.  A light is defined by position.  All lights have the same
 * intensity.
 *
 */
void
lib_output_light(center_pt)
    COORD4 center_pt;
{
    COORD3 vec;
    MATRIX txmat;
    double lscale;
    light_ptr new_light;

    if (center_pt[W] != 0.0)
	lscale = center_pt[W];
    else
	lscale = 1.0;

    COPY_COORD3(vec, center_pt)
    if (lib_tx_active()) {
	/* Perform transformations of the vertices and normals of
	   the polygon(s) */
	lib_get_current_tx(txmat);
	lib_transform_vector(vec, vec, txmat);
    }

    switch (gRT_out_format) {
	case OUTPUT_DELAYED:
	    new_light = (light_ptr)malloc(sizeof(struct light_struct));
	    if (new_light == NULL)
	       /* Quietly fail & return */
	       return;
	    COPY_COORD4(new_light->center_pt, center_pt);
	    new_light->center_pt[W] = lscale;
	    new_light->next = gLib_lights;
	    gLib_lights = new_light;
	    break;

	case OUTPUT_VIDEO:
	case OUTPUT_PLG:
	case OUTPUT_OBJ:
	case OUTPUT_RWX:
	    /* Not currently doing anything with lights */
	    break;

	case OUTPUT_NFF:
	    fprintf(gOutfile, "l %g %g %g\n",
		    vec[X], vec[Y], vec[Z]);
	    break;

	case OUTPUT_POVRAY_10:
	    tab_indent();
	    fprintf(gOutfile, "object {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "light_source {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "<%g %g %g>",
		    vec[X], vec[Y], vec[Z]);
	    fprintf(gOutfile, " color red %g green %g blue %g\n",
		    lscale, lscale, lscale);

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // light\n");

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // object\n");

	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POVRAY_20:
	    tab_indent();
	    fprintf(gOutfile, "light_source {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "<%g, %g, %g>",
		    vec[X], vec[Y], vec[Z]);
	    fprintf(gOutfile, " color red %g green %g blue %g\n",
		    lscale, lscale, lscale);

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // light\n");

	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POLYRAY:
	    tab_indent();
	    fprintf(gOutfile, "light <%g, %g, %g>, <%g, %g, %g>\n",
		    lscale, lscale, lscale,
		    vec[X], vec[Y], vec[Z]);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_VIVID:
	    tab_indent();
	    fprintf(gOutfile, "light {type point position %g %g %g",
		    vec[X], vec[Y], vec[Z]);
	    fprintf(gOutfile, " color %g %g %g }\n",
		    lscale, lscale, lscale);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_QRT:
	    tab_indent();
	    fprintf(gOutfile, "LAMP ( loc = (%g,%g,%g), dist = 0, radius = 1,",
		    vec[X], vec[Y], vec[Z]);
	    fprintf(gOutfile, " amb = (%g,%g,%g) )\n",
		    lscale, lscale, lscale);
	    break;

	case OUTPUT_RAYSHADE:
	    fprintf(gOutfile, "light %g point %g %g %g\n",
		    lscale, vec[X], vec[Y], vec[Z]);
	    break;

	case OUTPUT_RTRACE:
	    fprintf(gOutfile, "1 %g %g %g %g %g %g\n",
	    vec[X], vec[Y], vec[Z], lscale, lscale, lscale);
	    break;

	case OUTPUT_ART:
	    tab_indent();
	    fprintf(gOutfile, "light \n{");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "location(%g, %g, %g)  colour 0.5, 0.5, 0.5\n",
		   vec[X], vec[Y], vec[Z]);

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "}\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_RAWTRI:
	case OUTPUT_DXF:
	    break;

	case OUTPUT_RIB:
	  {
	    static int number= 0;

	    fprintf(gOutfile, "LightSource \"pointlight\" %d"
		    " \"from\" [ %#g %#g %#g ] \"intensity\" [20]\n", number++,
		    vec[X], vec[Y], vec[Z]);
	  }
	    break;
    }
}

/*-----------------------------------------------------------------*/
/*
 * Output background color.  A color is simply RGB (monitor dependent, but
 * that's life).
 */
void
lib_output_background_color(color)
    COORD3 color;
{
    switch (gRT_out_format) {
	case OUTPUT_VIDEO:
	case OUTPUT_DELAYED:
	case OUTPUT_PLG:
	case OUTPUT_OBJ:
	case OUTPUT_RWX:
	    COPY_COORD3(gBkgnd_color, color);
	    break;

	case OUTPUT_NFF:
	    fprintf(gOutfile, "b %g %g %g\n", color[X], color[Y], color[Z]);
	    break;

	case OUTPUT_POVRAY_10:
	    /* POV-Ray 1.0 does not support a background color */
	    /* Instead, create arbitrarily large enclosing sphere of that
	     * color */
	    tab_indent();
	    fprintf(gOutfile, "// background color:\n");

	    tab_indent();
	    fprintf(gOutfile, "object {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "sphere { <0 0 0> 9000  ");
	    fprintf(gOutfile,
	    "texture { ambient 1 diffuse 0 color red %g green %g blue %g } }\n",
		       color[X], color[Y], color[Z]);
	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // object - background\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POVRAY_20:
	    tab_indent();
	    fprintf(gOutfile, "background { color red %g green %g blue %g }\n",
		   color[X], color[Y], color[Z]);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POLYRAY:
	    tab_indent();
	    fprintf(gOutfile, "background <%g, %g, %g>\n",
		    color[X], color[Y], color[Z]);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_VIVID:
	    /* Vivid insists on putting the background into the studio */
	    tab_indent();
	    fprintf(gOutfile, "studio { background %g %g %g }\n",
		    color[X], color[Y], color[Z]);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_QRT:
	    tab_indent();
	    fprintf(gOutfile, "SKY ( horiz = (%g,%g,%g), zenith = (%g,%g,%g),",
		    color[X], color[Y], color[Z],
		    color[X], color[Y], color[Z]);
	    fprintf(gOutfile, " dither = 0 )\n");
	    break;

	case OUTPUT_RAYSHADE:
	    fprintf(gOutfile, "background %g %g %g\n",
		    color[X], color[Y], color[Z]);
	    break;

	case OUTPUT_RTRACE:
	    fprintf(gOutfile, "Colors\n");
	    fprintf(gOutfile, "%g %g %g\n", color[X], color[Y], color[Z]);
	    fprintf(gOutfile, "0 0 0\n");
	    break;

	case OUTPUT_RAWTRI:
	case OUTPUT_DXF:
	    break;

	case OUTPUT_ART:
	    tab_indent();
	    fprintf(gOutfile, "background %g, %g, %g\n",
		    color[X], color[Y], color[Z]);
	    fprintf(gOutfile, "\n");
	    break;
	  case OUTPUT_RIB:
	    fprintf(gOutfile, "# Background color [%#g %#g %#g]\n",
		    color[X], color[Y], color[Z]);
	    break;

    }
}

/*-----------------------------------------------------------------*/
static char *
create_surface_name(name, val)
    char *name;
    int val;
{
    char *txname;

    if (name != NULL)
	return name;

    txname = (char *)malloc(7*sizeof(char));
    if (txname == NULL)
	return NULL;
    sprintf(txname, "txt%03d", val);
    txname[6] = '\0';
    return txname;
}

/*-----------------------------------------------------------------*/
/*
 * Output color and shading parameters for all following objects
 *
 * For POV-Ray and Polyray, a character string will be returned that
 * identified this texture.  The default texture will be updated with
 * the name generated by this function.
 *
 * Meaning of the color and shading parameters:
 *    name   = name that this surface can be referenced by...
 *    color  = surface color
 *    ka     = ambient component
 *    kd     = diffuse component
 *    ks     = amount contributed from the reflected direction
 *    shine  = contribution from specular highlights
 *    ang    = angle at which the specular highlight falls to 50% of maximum
 *    t      = amount from the refracted direction
 *    i_of_r = index of refraction of the surface
 *
 */
char *
lib_output_color(name, color, ka, kd, ks, shine, ang, kt, i_of_r)
    char *name;
    COORD3 color;
    double ka, kd, ks, shine, ang, kt, i_of_r;
{
    surface_ptr new_surf;
    char *txname = NULL;
    double phong_pow;

    /* Increment the number of surface types we know about */
    ++gTexture_count;
    gTexture_ior = i_of_r;

    /* Calculate the Phong coefficient */
    phong_pow = PI * ang / 180.0;
    if (phong_pow <= 0.0)
	phong_pow = 100000.0;
    else if (phong_pow >= (PI/4.0))
	phong_pow = 0.000001;
    else
	phong_pow = -(log(2.0) / log(cos(2.0 * phong_pow)));

    switch (gRT_out_format) {
	case OUTPUT_DELAYED:
	    new_surf = (surface_ptr)malloc(sizeof(struct surface_struct));
	    if (new_surf == NULL)
	       /* Quietly fail */
		return NULL;
	    new_surf->surf_name = create_surface_name(name, gTexture_count);
	    new_surf->surf_index = gTexture_count;
	    COPY_COORD3(new_surf->color, color);
	    new_surf->ka = ka;
	    new_surf->kd = kd;
	    new_surf->ks = ks;
	    new_surf->shine = shine;
	    new_surf->ang = ang;
	    new_surf->kt = kt;
	    new_surf->ior = i_of_r;
	    new_surf->next = gLib_surfaces;
	    gLib_surfaces = new_surf;
	    break;

	case OUTPUT_PLG:
	case OUTPUT_VIDEO:
	    COPY_COORD3(gFgnd_color, color);
	    break;

	case OUTPUT_NFF:
	    fprintf(gOutfile, "f %g %g %g %g %g %g %g %g\n",
		color[X], color[Y], color[Z], kd, ks, phong_pow, kt, i_of_r);
	    break;

	case OUTPUT_POVRAY_10:
	    txname = create_surface_name(name, gTexture_count);
	    tab_indent();
	    fprintf(gOutfile, "#declare %s = texture {\n", txname);
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "color red %g green %g blue %g",
		    color[X], color[Y], color[Z]);
	    if (kt > 0)
		fprintf(gOutfile, " alpha %g", kt);
	    fprintf(gOutfile, "\n");

	    tab_indent();
	    fprintf(gOutfile, "ambient %g\n", ka);

	    tab_indent();
	    fprintf(gOutfile, "diffuse %g\n", kd);

	    if (shine != 0) {
		tab_indent();
		fprintf(gOutfile, "phong %g phong_size %g\n", shine, phong_pow);
	    }

	    if (ks != 0) {
		tab_indent();
		fprintf(gOutfile, "reflection %g\n", ks);
	    }

	    if (kt != 0) {
		tab_indent();
		fprintf(gOutfile, "refraction 1.0 ior %g\n", i_of_r);
	    }

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // texture %s\n", txname);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POVRAY_20:
	    txname = create_surface_name(name, gTexture_count);
	    tab_indent();
	    fprintf(gOutfile, "#declare %s = texture {\n", txname);
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "pigment {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "color red %g green %g blue %g",
		    color[X], color[Y], color[Z]);
	    if (kt > 0)
		fprintf(gOutfile, " filter %g", kt);
	    fprintf(gOutfile, "\n");

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // pigment\n");

	    tab_indent();
	    fprintf(gOutfile, "// normal { bumps, ripples, etc. }\n");

	    tab_indent();
	    fprintf(gOutfile, "finish {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "ambient %g\n", ka);

	    tab_indent();
	    fprintf(gOutfile, "diffuse %g\n", kd);

	    if (shine != 0) {
		tab_indent();
		fprintf(gOutfile, "phong %g  phong_size %g\n", shine, phong_pow);
	    }

	    if (ks != 0) {
		tab_indent();
		fprintf(gOutfile, "reflection %g\n", ks);
	    }

	    if (kt != 0) {
		tab_indent();
		fprintf(gOutfile, "refraction 1.0 ior %g\n", i_of_r);
	    }

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // finish\n");

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "} // texture %s\n", txname);
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_POLYRAY:
	    txname = create_surface_name(name, gTexture_count);
	    tab_indent();
	    fprintf(gOutfile, "define %s\n", txname);

	    tab_indent();
	    fprintf(gOutfile, "texture {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "surface {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "ambient <%g, %g, %g>, %g\n",
		   color[X], color[Y], color[Z], ka);

	    tab_indent();
	    fprintf(gOutfile, "diffuse <%g, %g, %g>, %g\n",
		   color[X], color[Y], color[Z], kd);

	    if (shine != 0) {
		tab_indent();
		fprintf(gOutfile, "specular white, %g\n", shine);
		tab_indent();
		fprintf(gOutfile, "microfacet Phong %g\n", ang);
	    }

	    if (ks != 0) {
		tab_indent();
		fprintf(gOutfile, "reflection white, %g\n", ks);
	    }

	    if (kt != 0) {
		tab_indent();
		fprintf(gOutfile, "transmission white, %g, %g\n", kt, i_of_r);
	    }

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "}\n");

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "}\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_VIVID:
	    tab_indent();
	    fprintf(gOutfile, "surface {\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "ambient %g %g %g\n",
		    ka * color[X], ka * color[Y], ka * color[Z], ka);

	    tab_indent();
	    fprintf(gOutfile, "diffuse %g %g %g\n",
		    kd * color[X], kd * color[Y], kd * color[Z], ka);

	    if (shine != 0) {
		tab_indent();
		fprintf(gOutfile, "shine %g %g %g %g\n",
		       phong_pow, shine, shine, shine);
	    }
	    if (ks != 0) {
		tab_indent();
		fprintf(gOutfile, "specular %g %g %g\n", ks, ks, ks);
	    }
	    if (kt != 0) {
		tab_indent();
		fprintf(gOutfile, "transparent %g %g %g\n",
		     kt * color[X], kt * color[Y], kt * color[Z]);
		tab_indent();
		fprintf(gOutfile, "ior %g\n", i_of_r);
	    }

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, "}\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_QRT:
	    tab_indent();
	    fprintf(gOutfile, "DEFAULT (\n");
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "amb = (%g,%g,%g),\n",
		    ka * color[X], ka * color[Y], ka * color[Z], ka);
	    tab_indent();
	    fprintf(gOutfile, "diff = (%g,%g,%g),\n",
		    kd * color[X], kd * color[Y], kd * color[Z], ka);
	    tab_indent();
	    fprintf(gOutfile, "reflect = %g, sreflect = %g,\n",
		    shine, phong_pow);
	    tab_indent();
	    fprintf(gOutfile, "mirror = (%g,%g,%g),\n",
		    ks * color[X], ks * color[Y], ks * color[Z]);
	    tab_indent();
	    fprintf(gOutfile, "trans = (%g,%g,%g), index = %g,\n",
		    kt * color[X], kt * color[Y], kt * color[Z], i_of_r);
	    tab_indent();
	    fprintf(gOutfile, "dither = 0\n");

	    tab_dec();
	    tab_indent();
	    fprintf(gOutfile, ")\n");
	    fprintf(gOutfile, "\n");
	    break;

	case OUTPUT_RAYSHADE:
	    txname = create_surface_name(name, gTexture_count);
	    tab_indent();
	    fprintf(gOutfile, "surface %s\n", txname);
	    tab_inc();

	    tab_indent();
	    fprintf(gOutfile, "ambient %g %g %g\n",
		   ka * color[X], ka * color[Y], ka * color[Z]);
	    tab_indent();
	    fprintf(gOutfile, "diffuse %g %g %g\n",
		   kd * color[X], kd * color[Y], kd * color[Z]);

	    if (shine != 0) {
		tab_indent();
		fprintf(gOutfile, "specular %g %g %g\n", shine, shine, shine);
		tab_indent();
		fprintf(gOutfile, "specpow %g\n", phong_pow);
	    }

	    if (ks != 0) {
		if (shine == 0.0) {
		    /* If there is no Phong highlighting, but there is
		       reflectivity, then we need to define the color of
		       specular reflections */
		    tab_indent();
		    fprintf(gOutfile, "specular 1.0 1.0 1.0\n");
		    tab_indent();
		    fprintf(gOutfile, "specpow 0.0\n");
		}
		tab_indent();
		fprintf(gOutfile, "reflect %g\n", ks);
	    }

	    if (kt != 0) {
		tab_indent();
		fprintf(gOutfile, "transp %g index %g\n", kt, i_of_r);
	    }

	    tab_dec();
	    tab_indent();
	    break;

	case OUTPUT_RTRACE:
	    if (shine > 0 && ks == 0.0) ks = shine;
	    fprintf(gOutfile, "1 %g %g %g %g %g %g %g %g %g %g 0 %g %g %g\n",
		    color[X], color[Y], color[Z],
		    kd, kd, kd,
		    ks, ks, ks,
		    (phong_pow > 100.0 ? 100.0 : phong_pow),
		    kt, kt, kt);
	    break;

	case OUTPUT_OBJ:
	    txname = create_surface_name(name, gTexture_count);
	    fprintf(gOutfile, "usemtl %s\n", txname);
	    break;

	case OUTPUT_RWX:
	    tab_indent();
	    fprintf(gOutfile, "Color %g %g %g\n",
		    color[X], color[Y], color[Z]);
	    tab_indent();
	    fprintf(gOutfile, "Surface %g %g %g\n",
		    ka, kd, ks);
	    tab_indent();
	    fprintf(gOutfile, "Opacity %g\n",
		    1.0-kt);
	   break;

	case OUTPUT_RAWTRI:
	    txname = create_surface_name(name, gTexture_count);
	    break;

	case OUTPUT_ART:
	    tab_indent();
	    fprintf(gOutfile, "colour %g, %g, %g\n",
		    color[X], color[Y], color[Z]);
	    tab_indent();
	    fprintf(gOutfile, "ambient %g, %g, %g\n",
		    color[X] * 0.05, color[Y] * 0.05, color[Z] * 0.05);

	    if (ks != 0.0) {
		tab_indent();
		fprintf(gOutfile, "material %g, %g, %g, %g\n",
			    i_of_r, kd, ks, phong_pow);
	    } else {
		tab_indent();
		fprintf(gOutfile, "material %g, %g, 0.0, 0.0\n",
			    i_of_r, kd);
	    }

	    tab_indent();
	    fprintf(gOutfile, "reflectance %g\n", ks);
	    tab_indent();
	    fprintf(gOutfile, "transparency %g\n", kt);
	    fprintf(gOutfile, "\n");
	    break;
	  case OUTPUT_RIB:
	    fprintf(gOutfile, "\n");
	    if (name != NULL)
	      fprintf(gOutfile, "Attribute \"identifier\" \"name\" \"%s\"\n",
		      name);
	    fprintf(gOutfile, "Color [ %#g %#g %#g ]\n",
		    color[X], color[Y], color[Z]);
	    fprintf(gOutfile, "Surface \"plastic\" \"Ka\" %#g \"Kd\" %#g"
		    " \"Ks\" %#g \"roughness\" %#g \n",
		    ka, kd, shine, 1.0/phong_pow);
	    break;
	  case OUTPUT_DXF:
	    break;
	  }

    /* Stash away the current texture name */
    gTexture_name = txname;

    return txname;
}
