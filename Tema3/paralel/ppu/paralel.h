/* Tema 3 ASC
 * Constantin Serban-Radoi 333CA
 * Aprilie 2013
 *
 * paralel.h - Header for macros and PNM functions declaration
 */

#ifndef PARALEL_H_
#define PARALEL_H_

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <libmisc.h>

#include "../debug.h"

#define PRINT_ERR_MSG_AND_EXIT(format, ...) \
	{ \
	fprintf(stderr, "%s:%d: " format, __func__, __LINE__, ##__VA_ARGS__); \
	fflush(stderr); \
	exit(1); \
	}

#define NUM_STREAMS 		16
#define MAX_FRAMES			100	/* there are at most 100 frames available */
#define MAX_PATH_LEN		256
#define IMAGE_TYPE_LEN 		2
#define SMALL_BUF_SIZE 		16
#define SCALE_FACTOR		4
#define NUM_CHANNELS		3	/* red, green and blue */
#define MAX_COLOR			255
#define NUM_IMAGES_WIDTH	4	/* the final big image has 4 small images */
#define NUM_IMAGES_HEIGHT	4	/* on the width and 4 on the height */

/* macros for easily accessing data */
#define GET_COLOR_VALUE(img, i, j, k) \
	((img)->data[((i) * (img->width) + (j)) * NUM_CHANNELS + (k)])
#define RED(img, i, j)		GET_COLOR_VALUE(img, i, j, 0)
#define GREEN(img, i, j)	GET_COLOR_VALUE(img, i, j, 1)
#define BLUE(img, i, j)		GET_COLOR_VALUE(img, i, j, 2)

/* macro for easily getting how much time has passed between two events */
#define GET_TIME_DELTA(t1, t2) ((t2).tv_sec - (t1).tv_sec + \
				((t2).tv_usec - (t1).tv_usec) / 1000000.0)


/* structure that is used to store an image into memory */
struct image {
	unsigned char *data;
	unsigned int width, height;
	int bullshit;
} __attribute__ ((aligned(16)));
/* data layout for an image:
 * if RED_i, GREEN_i, BLUE_i are the red, green and blue values for
 * the i-th pixel in the image than the data array inside struct image
 * looks like this:
 * RED_0 GREEN_0 BLUE_0 RED_1 GREEN_1 BLUE_1 RED_2 ...
*/

typedef struct ppu_data_t_ {
	int spe_id;				/* ID of the SPE */
	struct image *input;	/* Pointer to the input image */
	struct image *big_image;/* Pointer to the big_image */
	int num_frames;			/* Number of frames to be processed */
} __attribute__ ((aligned(16))) ppu_data_t;

enum mbox_type {
	BEGIN = 1,
	NOT_READY = 0,
	SPU_FRAME_DONE = 2
};

#define INFINITE -1
#define ONE_EVENT 1

/* ----------------------------------------------------------------------
 * PNM parsing functions
 */

/* allocate image data */
void alloc_image(struct image* img);

/* free image data */
void free_image(struct image* img);

/* read a pnm image */
void read_pnm(char* path, struct image* img);

/* write a pnm image */
void write_pnm(char* path, struct image* img);

#endif
