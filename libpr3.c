/*
 * libpr3.c - a library of primitive object output routines, part 3 of 3.
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


static unsigned int hfcount = 0;

/*-----------------------------------------------------------------*/
static char *
create_height_file(filename, height, width, data, type)
    char *filename;
    unsigned int height, width;
    float **data;
    int type;
{
    FILE *file;
    float v;
    unsigned int i, j;
    unsigned char r, g, b;
    unsigned char tgaheader[18];

    if (filename == NULL) {
	/* Need to create a new name for the height file */
	filename = malloc(10 * sizeof(char));
	if (filename == NULL) return NULL;
	sprintf(filename, "hf%03d.tga", hfcount++);
    }
    if ((file = fopen(filename, "wb")) == NULL) return NULL;
    if (type == 0) {
	/* Targa style height field for POV-Ray or Polyray */
	memset(tgaheader, 0, 18);
	tgaheader[2] = 2;
	tgaheader[12] = (unsigned char)(width & 0xFF);
	tgaheader[13] = (unsigned char)((width >> 8) & 0xFF);
	tgaheader[14] = (unsigned char)(height & 0xFF);
	tgaheader[15] = (unsigned char)((height >> 8) & 0xFF);
	tgaheader[16] = 24;
	tgaheader[17] = 0x20;
	fwrite(tgaheader, 18, 1, file);
	for (i=0;i<height;i++) {
	    PLATFORM_MULTITASK();
	    for (j=0;j<width;j++) {
		v = data[i][j];
		if (v < -128.0) v = -128.0;
		if (v > 127.0) v = 127.0;
		v += 128.0;
		r = v;
		v -= (float)r;
		g = (unsigned char)(256.0 * v);
		b = 0;
		fputc(b, file);
		fputc(g, file);
		fputc(r, file);
	    }
	}
    } else {
	/* Only square height fields in RayShade */
	if (height < width) width = height;
	else if (width < height) height = width;

	/* Start by storing the size as an int */
	fwrite(&height, sizeof(int), 1, file);

	/* Now store height values as native floats */
	for (i=0;i<height;i++)
	    for (j=0;j<width;j++)
		fwrite(&data[i][j], sizeof(float), 1, file);
    }
    fclose(file);

    return filename;
}


/*-----------------------------------------------------------------*/
void
lib_output_height(filename, data, height, width, x0, x1, y0, y1, z0, z1)
    char *filename;
    float **data;
    unsigned int height, width;
    double x0, x1;
    double y0, y1;
    double z0, z1;
{
    object_ptr new_object;

    if (gRT_out_format == OUTPUT_DELAYED) {
	filename = create_height_file(filename, height, width, data, 0);
	if (filename == NULL) return;

	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	if (new_object == NULL)
	    /* Quietly fail */
	    return;
	new_object->object_type  = HEIGHT_OBJ;
	new_object->curve_format = OUTPUT_CURVES;
	new_object->surf_index   = gTexture_count;
	new_object->object_data.height.width = width;
	new_object->object_data.height.height = height;
	new_object->object_data.height.data = data;
	new_object->object_data.height.filename = filename;
	new_object->object_data.height.x0 = x0;
	new_object->object_data.height.x1 = x1;
	new_object->object_data.height.y0 = y0;
	new_object->object_data.height.y1 = y1;
	new_object->object_data.height.z0 = z0;
	new_object->object_data.height.z1 = z1;
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;
    } else {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
	    case OUTPUT_NFF:
	    case OUTPUT_PLG:
	    case OUTPUT_QRT:
	    case OUTPUT_RTRACE:
	    case OUTPUT_VIVID:
	    case OUTPUT_RAWTRI:
	    case OUTPUT_RIB:
	    case OUTPUT_DXF:
		lib_output_polygon_height(height, width, data,
					  x0, x1, y0, y1, z0, z1);
		break;
	    case OUTPUT_POVRAY_10:
	    case OUTPUT_POVRAY_20:
		filename = create_height_file(filename, height, width, data, 0);
		if (filename == NULL) return;

		tab_indent();
		fprintf(gOutfile, "object {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, "height_field { tga \"%s\" }", filename);
		if (gRT_out_format == OUTPUT_POVRAY_10) {
		    tab_indent();
		    fprintf(gOutfile, "scale <%g %g %g>\n",
			    fabs(x1 - x0), fabs(y1 - y0), fabs(z1 - z0));
		    tab_indent();
		    fprintf(gOutfile, "translate <%g %g %g>\n", x0, y0, z0);
		} else {
		    tab_indent();
		    fprintf(gOutfile, "scale <%g, %g, %g>\n",
			    fabs(x1 - x0), fabs(y1 - y0), fabs(z1 - z0));
		    tab_indent();
		    fprintf(gOutfile, "translate <%g, %g, %g>\n", x0, y0, z0);
		}

		if (gTexture_name != NULL) {
		    tab_indent();
		    fprintf(gOutfile, "texture { %s }", gTexture_name);
		}

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "} // object - Height Field\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POLYRAY:
		filename = create_height_file(filename, height, width, data, 0);
		if (filename == NULL) return;
		tab_indent();
		fprintf(gOutfile, "object { height_field \"%s\" ", filename);
		fprintf(gOutfile, "scale <%g, 1, %g> ",
			fabs(x1-x0), fabs(z1-z0));
		fprintf(gOutfile, "translate <%g, %g, %g> ", x0, y0, z0);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RAYSHADE:
		filename = create_height_file(filename, height, width, data, 1);
		if (filename == NULL) return;
		fprintf(gOutfile, "heightfield ");
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, "\"%s\" ", filename);
		fprintf(gOutfile, "rotate 1 0 0 90 ");
		fprintf(gOutfile, "scale  %g 1 %g ",
			fabs(x1 - x0), fabs(z1 - z0));
		fprintf(gOutfile, "translate  %g %g %g ", x0, y0, z0);
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_ART:
		filename = create_height_file(filename, height, width, data, 1);
		if (filename == NULL) return;

		tab_indent();
		fprintf(gOutfile, "geometry {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, "translate(%g, %g, %g)\n", x0, y0, z0);
		tab_indent();
		fprintf(gOutfile, "scale(%g, 1, %g)\n",
			fabs(x1 - x0), fabs(z1 - z0));
		tab_indent();
		fprintf(gOutfile, "rotate(-90, x)\n");
		tab_indent();
		fprintf(gOutfile, "heightfield \"%s\"\n ", filename);

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;
	}
    }
}


/*-----------------------------------------------------------------*/
void
lib_output_torus(center, normal, iradius, oradius, curve_format)
    COORD3 center, normal;
    double iradius, oradius;
    int curve_format;
{
    object_ptr new_object;
    double len, xang, zang;

    if (gRT_out_format == OUTPUT_DELAYED) {
	/* Save all the pertinent information */
	new_object = (object_ptr)malloc(sizeof(struct object_struct));
	if (new_object == NULL)
	    /* Quietly fail */
	    return;
	new_object->object_type  = TORUS_OBJ;
	new_object->curve_format = curve_format;
	new_object->surf_index   = gTexture_count;
	COPY_COORD3(new_object->object_data.torus.center, center);
	COPY_COORD3(new_object->object_data.torus.normal, normal);
	new_object->object_data.torus.iradius = iradius;
	new_object->object_data.torus.oradius = oradius;
	new_object->next_object = gLib_objects;
	gLib_objects = new_object;
    } else if (curve_format == OUTPUT_CURVES) {
	switch (gRT_out_format) {
	    case OUTPUT_VIDEO:
	    case OUTPUT_NFF:
	    case OUTPUT_VIVID:
	    case OUTPUT_QRT:
	    case OUTPUT_POVRAY_10:
	    case OUTPUT_PLG:
	    case OUTPUT_RTRACE:
	    case OUTPUT_RAWTRI:
	    case OUTPUT_DXF:
		lib_output_polygon_torus(center, normal, iradius, oradius);
		break;
	    case OUTPUT_POVRAY_20:
		/*
		 A torus object lies in the x-z plane.  We need to determine
		 the angles of rotation to get it lined up with "normal".
		 */
		tab_indent();
		fprintf(gOutfile, "torus {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile, "%g, %g\n", iradius, oradius);

		(void)lib_normalize_vector(normal);
		len = sqrt(normal[X] * normal[X] + normal[Y] * normal[Y]);
		xang = 180.0 * asin(normal[Z]) / PI;
		if (len < EPSILON)
		    zang = 0.0;
		else
		    zang = -180.0 * acos(normal[Y] / len) / PI;
		if (normal[X] < 0)
		    zang = -zang;

		if (ABSOLUTE(xang) > EPSILON || ABSOLUTE(zang) > EPSILON) {
		    tab_indent();
		    fprintf(gOutfile, "rotate <%g, 0, %g>\n", xang, zang);
		}

		if (ABSOLUTE(center[X]) > EPSILON ||
		    ABSOLUTE(center[Y]) > EPSILON ||
		    ABSOLUTE(center[Z]) > EPSILON) {
		    tab_indent();
		    fprintf(gOutfile, "translate <%g, %g, %g>\n",
			    center[X], center[Y], center[Z]);
		}

		if (gTexture_name != NULL) {
		    tab_indent();
		    fprintf(gOutfile, "texture { %s }", gTexture_name);
		}
		fprintf(gOutfile, "\n");

		tab_dec();
		tab_indent();
		fprintf(gOutfile, "} // torus\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_POLYRAY:
		tab_indent();
		fprintf(gOutfile, "object { torus %g, %g", iradius, oradius);
		fprintf(gOutfile, ", <%g, %g, %g>, <%g, %g, %g>",
			center[X], center[Y], center[Z],
			normal[X], normal[Y], normal[Z]);
		if (gTexture_name != NULL)
		    fprintf(gOutfile, " %s", gTexture_name);
		fprintf(gOutfile, " }\n");
		fprintf(gOutfile, "\n");
		break;

	    case OUTPUT_RAYSHADE:
		fprintf(gOutfile, "torus ");
		if (gTexture_name != NULL)
		    fprintf(gOutfile, "%s ", gTexture_name);
		fprintf(gOutfile, " %g %g %g %g %g %g %g %g\n",
			iradius, oradius,
			center[X], center[Y], center[Z],
			normal[X], normal[Y], normal[Z]);
		break;

	    case OUTPUT_ART:
		tab_indent();
		fprintf(gOutfile, "torus {\n");
		tab_inc();

		tab_indent();
		fprintf(gOutfile,
		"radius %g  center(%g, %g, %g)  radius %g  center(%g, %g, %g)\n",
			  iradius, oradius,
			  center[X], center[Y], center[Z],
			  normal[X], normal[Y], normal[Z]);
		tab_dec();
		tab_indent();
		fprintf(gOutfile, "}\n");
		fprintf(gOutfile, "\n");
		break;
	}
    } else {
	lib_output_polygon_torus(center, normal, iradius, oradius);
    }
}

