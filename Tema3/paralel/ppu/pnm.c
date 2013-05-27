/* Tema 3 ASC
 * Constantin Serban-Radoi 333CA
 * Aprilie 2013
 *
 * pnm.c - Implementation of PNM functions
 */
#include "paralel.h"

/* read a character from a file specified by a descriptor */
static char read_char(int fd, char* path) {
	char c;
	int bytes_read;

	bytes_read = read(fd, &c, 1);
	if (bytes_read != 1){
		PRINT_ERR_MSG_AND_EXIT("Error reading from %s\n", path);
	}

	return c;
}

/* allocate image data */
void alloc_image(struct image* img) {
	//img->data = calloc(NUM_CHANNELS * img->width * img->height, sizeof(char));
	img->data = malloc_align(NUM_CHANNELS * img->width * img->height * sizeof(char), 4);

	if (!img->data){
		PRINT_ERR_MSG_AND_EXIT("Calloc failed\n");
	}
}

/* free image data */
void free_image(struct image* img) {
    if (img != NULL) {
        //free(img->data);
		free_align(img->data);
        img->data = NULL;
    }
}

/* read from fd until character c is found
 * result will be atoi(str) where str is what was read before c was
 * found
 */
static unsigned int read_until(int fd, char c, char* path) {

	char buf[SMALL_BUF_SIZE];
	int i;
	unsigned int res;

	i = 0;
	memset(buf, 0, SMALL_BUF_SIZE);
	buf[i] = read_char(fd, path);
	while (buf[i] != c){
		i++;
		if (i >= SMALL_BUF_SIZE){
			PRINT_ERR_MSG_AND_EXIT("Unexpected file format for %s\n", path);
		}
		buf[i] = read_char(fd, path);
	}
	res = atoi(buf);
	if (res <= 0) {
		PRINT_ERR_MSG_AND_EXIT("Result is %d when reading from %s\n",
			res, path);
	}

	return res;
}

/* read a pnm image */
void read_pnm(char* path, struct image* img) {
	int fd, bytes_read, bytes_left;
	char image_type[IMAGE_TYPE_LEN];
	unsigned char *ptr;
	unsigned int max_color;

	fd = open(path, O_RDONLY);

	if (fd < 0){
		PRINT_ERR_MSG_AND_EXIT("Error opening %s\n", path);
		exit(1);
	}

	/* read image type; should be P6 */
	bytes_read = read(fd, image_type, IMAGE_TYPE_LEN);
	if (bytes_read != IMAGE_TYPE_LEN){
		PRINT_ERR_MSG_AND_EXIT("Couldn't read image type for %s\n", path);
	}
	if (strncmp(image_type, "P6", IMAGE_TYPE_LEN)){
		PRINT_ERR_MSG_AND_EXIT("Expecting P6 image type for %s. Got %s\n",
			path, image_type);
	}

	/* read \n */
	read_char(fd, path);

	/* read width, height and max color value */
	img->width = read_until(fd, ' ', path);
	img->height = read_until(fd, '\n', path);
	max_color = read_until(fd, '\n', path);
	if (max_color != MAX_COLOR){
		PRINT_ERR_MSG_AND_EXIT("Unsupported max color value %d for %s\n",
			max_color, path);
	}

	/* allocate image data */
	alloc_image(img);

	/* read the actual data */
	bytes_left = img->width * img->height * NUM_CHANNELS;
	ptr = img->data;
	while (bytes_left > 0){
		bytes_read = read(fd, ptr, bytes_left);
		if (bytes_read <= 0){
			PRINT_ERR_MSG_AND_EXIT("Error reading from %s\n", path);
		}
		ptr += bytes_read;
		bytes_left -= bytes_read;
	}

	close(fd);
}

/* write a pnm image */
void write_pnm(char* path, struct image* img) {
	int fd, bytes_written, bytes_left;
	char buf[32];
	unsigned char* ptr;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0){
		PRINT_ERR_MSG_AND_EXIT("Error opening %s\n", path);
	}

	/* write image type, image width, height and max color */
	sprintf(buf, "P6\n%d %d\n%d\n", img->width, img->height, MAX_COLOR);
	ptr = (unsigned char*)buf;
	bytes_left = strlen(buf);
	while (bytes_left > 0){
		bytes_written = write(fd, ptr, bytes_left);
		if (bytes_written <= 0){
			PRINT_ERR_MSG_AND_EXIT("Error writing to %s\n", path);
		}
		bytes_left -= bytes_written;
		ptr += bytes_written;
	}

	/* write the actual data */
	ptr = img->data;
	bytes_left = img->width * img->height * NUM_CHANNELS;
	while (bytes_left > 0){
		bytes_written = write(fd, ptr, bytes_left);
		if (bytes_written <= 0){
			PRINT_ERR_MSG_AND_EXIT("Error writing to %s\n", path);
		}
		bytes_left -= bytes_written;
		ptr += bytes_written;
	}

	close(fd);
}
