/* sombrero.c - example of the use of heightfield output
   From: Alexander Enzmann <70323.2461@compuserve.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lib.h"

/* Standard arguments */
static int size_factor = 1;
static int raytracer_format ;
static int output_format    = OUTPUT_CURVES;

/* Define constants for the sombrero function */
static double a_const, b_const, c_const, two_pi_a;

static float **
create_sombrero(width, height, x0, x1, z0, z1)
   unsigned width, height;
   double x0, x1, z0, z1;
{
   float **data;
   double x, deltax, z, deltaz;
   unsigned i, j;

   a_const = 1.0;
   b_const = 1.0;
   c_const = 3.0;
   two_pi_a = 2.0 * 3.14159265358 * a_const;

   deltax = (x1 - x0) / (double)width;
   deltaz = (z1 - z0) / (double)height;

   if ((data = malloc(height * sizeof(float *))) == NULL) {
      fprintf(stderr, "HF allocation failed\n");
      exit(1);
      }
   for (i=0,z=z0;i<height;i++,z+=deltaz) {
      if ((data[i] = malloc(width * sizeof(float ))) == NULL) {
	 fprintf(stderr, "HF allocation failed\n");
	 exit(1);
	 }
      for (j=0,x=x0;j<width;j++,x+=deltax)
	 data[i][j] = c_const * cos(two_pi_a * sqrt(x * x + z * z)) *
		      exp(-b_const * sqrt(x * x + z * z));
      }
   return data;
}

main(argc, argv)
   int argc;
   char *argv[];
{
   COORD4 back_color, obj_color;
   COORD4 from, at, up, light;
   unsigned width = 64, height = 64;
   float **data;

   /* Start by defining which raytracer we will be using */
   if (lib_gen_get_opts(argc, argv, &size_factor, &raytracer_format,
			&output_format))
      return EXIT_FAIL;

   if (lib_open(raytracer_format, "sombrero.out"))
      return EXIT_FAIL;

   lib_set_output_file(stdout);
   lib_set_raytracer(raytracer_format);
   lib_set_polygonalization(3, 3);

   /* output viewpoint */
   SET_COORD3(from, 0, 5, -8);
   SET_COORD3(at, 0, 0, 0);
   SET_COORD3(up, 0, 1, 1);
   lib_output_viewpoint(from, at, up, 40.0, 1.0, 0.01, 512, 512);

   /* output background color - UNC sky blue */
   SET_COORD3(back_color, 0.078, 0.361, 0.753);
   lib_output_background_color(back_color);

   /* output light sources */
   SET_COORD4(light, 10, 10, -10, 1);
   lib_output_light(light);

   /* Height field color - shiny_red */
   SET_COORD3(obj_color, 1, 0.1, 0.1);
   lib_output_color(NULL, obj_color, 0.1, 0.8, 0, 0.5, 20, 0, 1);

   /* Create the height field */
   data = create_sombrero(width, height, -4.0, 4.0, -4.0, 4.0);
   lib_output_height(NULL, data, width, height, -4, 4, -3, 3, -4, 4);

   lib_close();

   return EXIT_SUCCESS;
}


