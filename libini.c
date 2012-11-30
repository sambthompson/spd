/*
 * libini.c - initialization and teardown routines.
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
/* Library initialization/teardown functions */
/*-----------------------------------------------------------------*/
int
lib_open( raytracer_format, filename )
int     raytracer_format ;
char    *filename ;     /* unused except for Mac version */
{
#ifdef OUTPUT_TO_FILE
    /* no stdout, so write to a file! */
    if (raytracer_format == OUTPUT_VIDEO) {
	gStdout_file = stdout;
    } else {
	gStdout_file = fopen(filename,"w");
	if ( gStdout_file == NULL ) return 1 ;
    }
#endif /* OUTPUT_TO_FILE */

    lib_set_output_file(gStdout_file);

    gRT_orig_format = raytracer_format;
    if ((raytracer_format == OUTPUT_RTRACE) ||
	(raytracer_format == OUTPUT_PLG))
	lib_set_raytracer(OUTPUT_DELAYED);
    else if (raytracer_format == OUTPUT_RWX) {
       fprintf(gOutfile, "ModelBegin\n");
       fprintf(gOutfile, "ClumpBegin\n");
       fprintf(gOutfile, "LightSampling Vertex\n");
       lib_set_raytracer(raytracer_format);
       }
    else
	lib_set_raytracer(raytracer_format);


    return 0 ;
}

/*-----------------------------------------------------------------*/
void
lib_close PARAMS((void))
{
    /* Make sure everything is cleaned up */
    if ((gRT_orig_format == OUTPUT_RTRACE) ||
	(gRT_orig_format == OUTPUT_PLG)) {
	lib_set_raytracer(gRT_orig_format);
	lib_flush_definitions();
    }

    if (gRT_out_format == OUTPUT_RIB) {
	fprintf(gOutfile, "WorldEnd\n");
	fprintf(gOutfile, "FrameEnd\n");
    }
    else if (gRT_out_format == OUTPUT_DXF) {
	fprintf(gOutfile, "  0\n");
	fprintf(gOutfile, "ENDSEC\n");
	fprintf(gOutfile, "  0\n");
	fprintf(gOutfile, "EOF\n");
    }
    else if (gRT_out_format == OUTPUT_RWX) {
	fprintf(gOutfile, "ClumpEnd\n");
	fprintf(gOutfile, "ModelEnd\n");
    }

#ifdef OUTPUT_TO_FILE
    /* no stdout, so close our output! */
    if (gStdout_file)
	fclose(gStdout_file);
#endif /* OUTPUT_TO_FILE */
    if (gRT_out_format == OUTPUT_VIDEO)
	display_close(1);
}


/*-----------------------------------------------------------------*/
void lib_storage_initialize PARAMS((void))
{
    gPoly_vbuffer = (unsigned int*)malloc(VBUFFER_SIZE * sizeof(unsigned int));
    gPoly_end = (int*)malloc(POLYEND_SIZE * sizeof(int));
    if (!gPoly_vbuffer || !gPoly_end) {
	fprintf(stderr,
		"Error(lib_storage_initialize): Can't allocate memory.\n");
	exit(1);
    }
} /* lib_storage_initialize */


/*-----------------------------------------------------------------*/
void
lib_storage_shutdown PARAMS((void))
{
    if (gPoly_vbuffer) {
	free(gPoly_vbuffer);
	gPoly_vbuffer = NULL;
    }
    if (gPoly_end) {
	free(gPoly_end);
	gPoly_end = NULL;
    }
} /* lib_storage_shutdown */


/*-----------------------------------------------------------------*/
void show_gen_usage PARAMS((void))
{
    /* localize the usage strings to be expanded only once for space savings */
#if defined(applec) || defined(THINK_C)
    /* and don't write to stdout on Macs, which don't have console I/O, and  */
    /* won't ever get this error anyway, since parms are auto-generated.     */
#else

    fprintf(stderr, "usage [-s size] [-r format] [-c|t]\n");
    fprintf(stderr, "-s size - input size of database\n");
    fprintf(stderr, "-r format - input database format to output:\n");
    fprintf(stderr, "   0   Output direct to the screen (sys dependent)\n");
    fprintf(stderr, "   1   NFF - MTV\n");
    fprintf(stderr, "   2   POV-Ray 1.0\n");
    fprintf(stderr, "   3   Polyray v1.4, v1.5\n");
    fprintf(stderr, "   4   Vivid 2.0\n");
    fprintf(stderr, "   5   QRT 1.5\n");
    fprintf(stderr, "   6   Rayshade\n");
    fprintf(stderr, "   7   POV-Ray 2.0 (format is subject to change)\n");
    fprintf(stderr, "   8   RTrace 8.0.0\n");
    fprintf(stderr, "   9   PLG format for use with rend386\n");
    fprintf(stderr, "   10  Raw triangle output\n");
    fprintf(stderr, "   11  art 2.3\n");
    fprintf(stderr, "   12  RenderMan RIB format\n");
    fprintf(stderr, "   13  Autodesk DXF format (polygons only)\n");
    fprintf(stderr, "   14  Wavefront OBJ format (polygons only)\n");
    fprintf(stderr, "   15  RenderWare RWX script file\n");
    fprintf(stderr, "-c - output true curved descriptions\n");
    fprintf(stderr, "-t - output tessellated triangle descriptions\n");

#endif
} /* show_gen_usage */

void show_read_usage PARAMS((void))
{
    /* localize the usage strings to be expanded only once for space savings */
#if defined(applec) || defined(THINK_C)
    /* and don't write to stdout on Macs, which don't have console I/O, and  */
    /* won't ever get this error anyway, since parms are auto-generated.     */
#else

    fprintf(stderr, "usage [-f filename] [-r format] [-c|t]\n");
    fprintf(stderr, "-f filename - file to import/convert/display\n");
    fprintf(stderr, "-r format - format to output:\n");
    fprintf(stderr, "   0   Output direct to the screen (sys dependent)\n");
    fprintf(stderr, "   1   NFF - MTV\n");
    fprintf(stderr, "   2   POV-Ray 1.0\n");
    fprintf(stderr, "   3   Polyray v1.4, v1.5\n");
    fprintf(stderr, "   4   Vivid 2.0\n");
    fprintf(stderr, "   5   QRT 1.5\n");
    fprintf(stderr, "   6   Rayshade\n");
    fprintf(stderr, "   7   POV-Ray 2.0 (format is subject to change)\n");
    fprintf(stderr, "   8   RTrace 8.0.0\n");
    fprintf(stderr, "   9   PLG format for use with rend386\n");
    fprintf(stderr, "   10  Raw triangle output\n");
    fprintf(stderr, "   11  art 2.3\n");
    fprintf(stderr, "   12  RenderMan RIB format\n");
    fprintf(stderr, "   13  Autodesk DXF format (polygons only)\n");
    fprintf(stderr, "   14  Wavefront OBJ format (polygons only)\n");
    fprintf(stderr, "   15  RenderWare RWX script file\n");
    fprintf(stderr, "-c - output true curved descriptions\n");
    fprintf(stderr, "-t - output tessellated triangle descriptions\n");

#endif
} /* show_read_usage */


/*-----------------------------------------------------------------*/
/*
 * Command line option parser for db generator
 *
 * -s size - input size of database (1 to N)
 * -r format - input database format to output
 *    0  Output direct to the screen (sys dependent)
 *    1  NFF - MTV
 *    2  POV-Ray 1.0
 *    3  Polyray v1.4, v1.5
 *    4  Vivid 2.0
 *    5  QRT 1.5
 *    6  Rayshade
 *    7  POV-Ray 2.0 (format is subject to change)
 *    8  RTrace 8.0.0
 *    9  PLG format for use with "rend386"
 *   10  Raw triangle output
 *   11  art 2.3
 *   12  RenderMan RIB format
 *   13  Autodesk DXF format
 *   14  Wavefront OBJ format
 *   15  RenderWare RWX format
 * -c - output true curved descriptions
 * -t - output tessellated triangle descriptions
 *
 * TRUE returned if bad command line detected
 * some of these are useless for the various routines - we're being a bit
 * lazy here...
 */
int     lib_gen_get_opts( argc, argv, p_size, p_rdr, p_curve )
int     argc ;
char    *argv[] ;
int     *p_size, *p_rdr, *p_curve ;
{
int num_arg ;
int val ;

    num_arg = 0 ;

    while ( ++num_arg < argc ) {
	if ( (*argv[num_arg] == '-') || (*argv[num_arg] == '/') ) {
	    switch( argv[num_arg][1] ) {
		case 'c':       /* true curve output */
		    *p_curve = OUTPUT_CURVES ;
		    break ;
		case 't':       /* tessellated curve output */
		    *p_curve = OUTPUT_PATCHES ;
		    break ;
		case 'r':       /* renderer selection */
		    if ( ++num_arg < argc ) {
			sscanf( argv[num_arg], "%d", &val ) ;
			if ( val < OUTPUT_VIDEO || val >= OUTPUT_DELAYED ) {
			    fprintf( stderr,
				    "bad renderer value %d given\n",val);
			    show_gen_usage();
			    return( TRUE ) ;
			}
			*p_rdr = val ;
		    } else {
			fprintf( stderr, "not enough args for -r option\n" ) ;
			show_gen_usage();
			return( TRUE ) ;
		    }
		    break ;
		case 's':       /* size selection */
		    if ( ++num_arg < argc ) {
			sscanf( argv[num_arg], "%d", &val ) ;
			if ( val < 1 ) {
			    fprintf( stderr,
				    "bad size value %d given\n",val);
			    show_gen_usage();
			    return( TRUE ) ;
			}
			*p_size = val ;
		    } else {
			fprintf( stderr, "not enough args for -s option\n" ) ;
			show_gen_usage();
			return( TRUE ) ;
		    }
		    break ;
		default:
		    fprintf( stderr, "unknown argument -%c\n",
			    argv[num_arg][1] ) ;
		    show_gen_usage();
		    return( TRUE ) ;
	    }
	} else {
	    fprintf( stderr, "unknown argument %s\n",
		    argv[num_arg] ) ;
	    show_gen_usage();
	    return( TRUE ) ;
	}
    }
    return( FALSE ) ;
}
/*-----------------------------------------------------------------*/
/*
 * Command line option parser for db reader (converter/displayer)
 *
 * -f filename - file to import/convert/display
 * -r format - input database format to output
 *    0  Output direct to the screen (sys dependent)
 *    1  NFF - MTV
 *    2  POV-Ray 1.0
 *    3  Polyray v1.4, v1.5
 *    4  Vivid 2.0
 *    5  QRT 1.5
 *    6  Rayshade
 *    7  POV-Ray 2.0 (format is subject to change)
 *    8  RTrace 8.0.0
 *    9  PLG format for use with "rend386"
 *   10  Raw triangle output
 *   11  art 2.3
 *   12  RenderMan RIB format
 *   13  Autodesk DXF format
 *   14  Wavefront OBJ format
 *   15  RenderWare RWX format
 * -c - output true curved descriptions
 * -t - output tessellated triangle descriptions
 *
 * TRUE returned if bad command line detected
 * some of these are useless for the various routines - we're being a bit
 * lazy here...
 */
int     lib_read_get_opts( argc, argv, p_rdr, p_curve, p_infname )
int     argc ;
char    *argv[] ;
int     *p_rdr, *p_curve ;
char *p_infname;
{
int num_arg ;
int val ;

    num_arg = 0 ;

    while ( ++num_arg < argc ) {
	if ( (*argv[num_arg] == '-') || (*argv[num_arg] == '/') ) {
	    switch( argv[num_arg][1] ) {
		case 'c':       /* true curve output */
		    *p_curve = OUTPUT_CURVES ;
		    break ;
		case 't':       /* tessellated curve output */
		    *p_curve = OUTPUT_PATCHES ;
		    break ;
		case 'f':       /* input file name */
		    if ( p_infname == NULL ) {
			fprintf( stderr, "-f option not allowed\n" ) ;
			show_read_usage();
			return( TRUE ) ;
		    } else {
			if ( ++num_arg < argc ) {
			    sscanf( argv[num_arg], "%s", p_infname ) ;
			} else {
			    fprintf( stderr, "not enough args for -f option\n" ) ;
			    show_read_usage();
			    return( TRUE ) ;
			}
		    }
		    break ;
		case 'r':       /* renderer selection */
		    if ( ++num_arg < argc ) {
			sscanf( argv[num_arg], "%d", &val ) ;
			if ( val < OUTPUT_VIDEO || val >= OUTPUT_DELAYED ) {
			    fprintf( stderr,
				    "bad renderer value %d given\n",val);
			    show_read_usage();
			    return( TRUE ) ;
			}
			*p_rdr = val ;
		    } else {
			fprintf( stderr, "not enough args for -r option\n" ) ;
			show_read_usage();
			return( TRUE ) ;
		    }
		    break ;
		default:
		    fprintf( stderr, "unknown argument -%c\n",
			    argv[num_arg][1] ) ;
		    show_read_usage();
		    return( TRUE ) ;
	    }
	} else {
	    fprintf( stderr, "unknown argument %s\n",
		    argv[num_arg] ) ;
	    show_read_usage();
	    return( TRUE ) ;
	}
    }
    return( FALSE ) ;
}


/*-----------------------------------------------------------------*/
void
lib_clear_database()
{
    surface_ptr ts1, ts2;
    object_ptr to1, to2;
    light_ptr tl1, tl2;

    gOutfile = stdout;
    gTexture_name = NULL;
    gTexture_count = 0;
    gObject_count = 0;
    gTexture_ior = 1.0;
    gRT_out_format = OUTPUT_RT_DEFAULT;
    gU_resolution = OUTPUT_RESOLUTION;
    gV_resolution = OUTPUT_RESOLUTION;
    SET_COORD3(gBkgnd_color, 0.0, 0.0, 0.0);
    SET_COORD3(gFgnd_color, 0.0, 0.0, 0.0);

    /* Remove all surfaces */
    ts1 = gLib_surfaces;
    while (ts1 != NULL) {
	ts2 = ts1;
	ts1 = ts1->next;
	free(ts2->surf_name);
	free(ts2);
    }
    gLib_surfaces = NULL;

    /* Remove all objects */
    to1 = gLib_objects;
    while (to1 != NULL) {
	to2 = to1;
	to1 = to1->next_object;
	free(to2);
    }
    gLib_objects = NULL;

    /* Remove all lights */
    tl1 = gLib_lights;
    while (tl1 != NULL) {
	tl2 = tl1;
	tl1 = tl1->next;
	free(tl2);
    }
    gLib_lights = NULL;

    /* Reset the view */

    /* Deallocate polygon buffer */
    if (gPoly_vbuffer != NULL)
	lib_storage_shutdown();

    /* Clear vertex counters for polygons */
    gVertex_count = 0; /* Vertex coordinates */
    gNormal_count = 0; /* Vertex normals */

    /* Clear out the polygon stack */
    to1 = gPolygon_stack;
    while (to1 != NULL) {
	to2 = to1;
	to1 = to1->next_object;
	free(to2);
    }
    gPolygon_stack = NULL;
}

/*-----------------------------------------------------------------*/
void
lib_flush_definitions()
{
    switch (gRT_out_format) {
       case OUTPUT_RTRACE:
       case OUTPUT_VIDEO:
       case OUTPUT_NFF:
       case OUTPUT_POVRAY_10:
       case OUTPUT_POLYRAY:
       case OUTPUT_VIVID:
       case OUTPUT_QRT:
       case OUTPUT_RAYSHADE:
       case OUTPUT_POVRAY_20:
       case OUTPUT_PLG:
       case OUTPUT_OBJ:
       case OUTPUT_RWX:
       case OUTPUT_RAWTRI:
	    lib_output_viewpoint(gViewpoint.from, gViewpoint.at, gViewpoint.up, gViewpoint.angle,
		    gViewpoint.aspect, gViewpoint.hither, gViewpoint.resx, gViewpoint.resy);

	    lib_output_background_color(gBkgnd_color);

	    dump_all_lights();

	    dump_reorder_surfaces();

	    dump_all_surfaces();

	    dump_all_objects();

	    if (gRT_out_format == OUTPUT_RTRACE)
		fprintf(gOutfile, "Textures\n\n");

	    break;
       case OUTPUT_DELAYED:
	    fprintf(stderr, "Error: Renderer not selected before flushing\n");
	    exit(1);
    }

    if (gRT_out_format == OUTPUT_PLG)
	/* An extra step is needed to build the polygon file. */
	dump_plg_file();
}
