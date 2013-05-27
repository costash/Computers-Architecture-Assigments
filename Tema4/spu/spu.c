/* Constantin Serban-Radoi 333CA
 * Tema 4 ASC - Mai 2013
 * SPU program
 */
#include <spu_mfcio.h>
#include <stdio.h>
#include <libmisc.h>
#include <string.h>

#include <spu_intrinsics.h>


#include "../common.h"

#define MY_TAG 5
#define MY_TAG2 6
#define DONE 1

#define LINES_PER_ELEM 8
#define NUM_LIST_ELEMS 4
#define NUM_OUT_ELEMS 8

#define waittag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();

uint32_t dec_end;
#define DECREMENTER_INIT ((uint32_t)0xFFFFFFFF)

void process_image_dmalist(struct image* img) {
unsigned char *input;/* Input images lines */
	unsigned char *output;	/* Output lines */
	unsigned char *temp;	/* temporary line */
	unsigned int i, j, k, out_line, r, g, b;
	unsigned int addr1, addr2;
	int block_nr = img->block_nr;
	vector unsigned char *v1, *v2, *v3, *v4;

	const unsigned int LINE_SIZE = img->width * NUM_CHANNELS;
	const unsigned int FOUR_LINES = SCALE_FACTOR * LINE_SIZE;
	const unsigned int LIST_CHUNK = NUM_LIST_ELEMS * LINE_SIZE * LINES_PER_ELEM;
	vector unsigned char *vec_temp;

	input = malloc_align(LIST_CHUNK, 4);
	output = malloc_align(LINES_PER_ELEM * LINE_SIZE / SCALE_FACTOR, 4);
	temp = malloc_align(LINES_PER_ELEM * LINE_SIZE, 4);

	vector unsigned char * vec1[LINES_PER_ELEM];
	vector unsigned char * vec2[LINES_PER_ELEM];
	vector unsigned char * vec3[LINES_PER_ELEM];
	vector unsigned char * vec4[LINES_PER_ELEM];
	vector unsigned char * vec5[LINES_PER_ELEM];
	for (i = 0; i < LINES_PER_ELEM; ++i) {
		vec1[i] = (vector unsigned char *) (input + i * FOUR_LINES);
		vec2[i] = (vector unsigned char *) (input + LINE_SIZE + i * FOUR_LINES);
		vec3[i] = (vector unsigned char *) (input + 2 * LINE_SIZE + i * FOUR_LINES);
		vec4[i] = (vector unsigned char *) (input + 3 * LINE_SIZE + i * FOUR_LINES);
		vec5[i] = (vector unsigned char *) (temp + i * FOUR_LINES);
	}

	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS *
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	addr1 = (unsigned int)img->src; //start of the image


	/* DMA List setup */
	mfc_list_element_t in_list[NUM_LIST_ELEMS];
	for (i = 0; i < NUM_LIST_ELEMS; ++i) {
		in_list[i].size = LINE_SIZE * LINES_PER_ELEM;
		in_list[i].eal = addr1 + i * LINE_SIZE * LINES_PER_ELEM;
	}

	// Ar trebui folosite la mfc_putl, dar nu sunt suficient de eficiente
	/*mfc_list_element_t out_list[NUM_OUT_ELEMS];
	for (i = 0; i < NUM_OUT_ELEMS; ++i) {
		out_list[i].size = LINE_SIZE / SCALE_FACTOR;
		out_list[i].eal = addr2 + i * LINE_SIZE;
	}*/


	spu_write_decrementer(DECREMENTER_INIT);

	for (i = 0; i < img->height / 32; ++i) {
		/* Get 32 lines */
		dprintf("i is %u before getl\n", i);
		mfc_getl(input, 0, in_list, sizeof(in_list), MY_TAG, 0, 0);
		dprintf("after getl\n");

		dprintf("before wait\n");
		waittag(MY_TAG);
		dprintf("after wait\n");

		for (j = 0; j < NUM_OUT_ELEMS; ++j) {
			in_list[j].eal += LIST_CHUNK;
		}

		// Compute the scaled lines
		for (out_line = 0; out_line < 8; ++out_line) {
			v1 = vec1[out_line];
			v2 = vec2[out_line];
			v3 = vec3[out_line];
			v4 = vec4[out_line];
			vec_temp = vec5[out_line];
			for (j = 0; j < img->width * NUM_CHANNELS / 16; ++j) {
				vec_temp[j] = spu_avg(spu_avg(v1[j], v2[j]), spu_avg(v3[j], v4[j]));
			}

			for (j = 0; j < img->width; j += SCALE_FACTOR) {
				r = g = b = 0;
				for (k = j; k < j + SCALE_FACTOR; k++) {
					r += temp[out_line * FOUR_LINES + k * NUM_CHANNELS + 0];
					g += temp[out_line * FOUR_LINES + k * NUM_CHANNELS + 1];
					b += temp[out_line * FOUR_LINES + k * NUM_CHANNELS + 2];
				}
				r /= SCALE_FACTOR;
				b /= SCALE_FACTOR;
				g /= SCALE_FACTOR;

				output[out_line * FOUR_LINES + j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
				output[out_line * FOUR_LINES + j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
				output[out_line * FOUR_LINES + j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
			}

			//put the scaled line back
			mfc_put(output + out_line * FOUR_LINES, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
			addr2 += LINE_SIZE; //line inside spu block
		}

		// E MULT MAI PROST ASA decat cu put-ul de mai sus
		//mfc_putl(output, 0, out_list, sizeof(out_list), MY_TAG, 0, 0);
		//for (j = 0; j < NUM_OUT_ELEMS; ++j) {
			//out_list[j].eal += LINE_SIZE * NUM_OUT_ELEMS;
		//}
		waittag(MY_TAG);
	}
	dec_end = spu_read_decrementer();
	//dprintf("%u\n", dec_end);
	free_align(temp);
	free_align(input);
	free_align(output);
}

void process_image_double(struct image* img) {
	unsigned char *input[2], *output[2], *temp[2];
	unsigned int addr1, addr2, i, j, k, r, g, b;
	int block_nr = img->block_nr;

	const unsigned int LINE_SIZE = img->width * NUM_CHANNELS;
	const unsigned int FOUR_LINES = SCALE_FACTOR * LINE_SIZE;

	const int tags[2] = {MY_TAG, MY_TAG2};

	int cur_buf = 0;

	input[0] = malloc_align(FOUR_LINES, 4);
	input[1] = malloc_align(FOUR_LINES, 4);
	output[0] = malloc_align(LINE_SIZE / SCALE_FACTOR, 4);
	output[1] = malloc_align(LINE_SIZE / SCALE_FACTOR, 4);
	temp[0] = malloc_align(LINE_SIZE, 4);
	temp[1] = malloc_align(LINE_SIZE, 4);

	vector unsigned char * const v1[2] = {
			(vector unsigned char *) input[0],
			(vector unsigned char *) input[1]
	};
	vector unsigned char * const v2[2] = {
			(vector unsigned char *) (input[0] + LINE_SIZE),
			(vector unsigned char *) (input[1] + LINE_SIZE)
	};
	vector unsigned char * const v3[2] = {
			(vector unsigned char *) (input[0] + 2 * LINE_SIZE),
			(vector unsigned char *) (input[1] + 2 * LINE_SIZE)
	};
	vector unsigned char * const v4[2] = {
			(vector unsigned char *) (input[0] + 3 * LINE_SIZE),
			(vector unsigned char *) (input[1] + 3 * LINE_SIZE)
	};
	vector unsigned char * const v5[2] = {
			(vector unsigned char *) temp[0],
			(vector unsigned char *) temp[1]
	};

	vector unsigned char *vv1, *vv2, *vv3, *vv4, *vv5;
	unsigned char *poutput, *ptemp;

	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS *
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	addr1 = (unsigned int)img->src; //start of the image

	dprintf("First line addr1 %x input %p\n", addr1, input[cur_buf]);

	spu_write_decrementer(DECREMENTER_INIT);

	/* Get the first chunk of 4 lines for double buffering */
	mfc_get(input[cur_buf], addr1, FOUR_LINES, tags[cur_buf], 0, 0);
	addr1 += FOUR_LINES;

	for (i = 1; i < img->height / SCALE_FACTOR; ++i, cur_buf = !cur_buf){
		//get 4 lines
		dprintf("Line %u addr1 %u input %p\n", i, addr1, input[!cur_buf]);
		mfc_get(input[!cur_buf], addr1, FOUR_LINES, tags[!cur_buf], 0, 0);
		addr1 += FOUR_LINES;

		vv1 = v1[cur_buf];
		vv2 = v2[cur_buf];
		vv3 = v3[cur_buf];
		vv4 = v4[cur_buf];
		vv5 = v5[cur_buf];
		poutput = output[cur_buf];
		ptemp = temp[cur_buf];

		waittag(tags[cur_buf]);
		dprintf("After waittag\n");

		//compute the scaled line
		for (j = 0; j < LINE_SIZE / 16; j++) {
			dprintf("Before avg line %u j = %u\n", i - 1, j);
			vv5[j] = spu_avg(spu_avg(vv1[j], vv2[j]), spu_avg(vv3[j], vv4[j]));
			dprintf("@avg line %u j = %u\n", i - 1, j);
		}
		for (j = 0; j < img->width; j += SCALE_FACTOR){
			r = g = b = 0;
			for (k = j; k < j + SCALE_FACTOR; k++) {
				r += ptemp[k * NUM_CHANNELS + 0];
				g += ptemp[k * NUM_CHANNELS + 1];
				b += ptemp[k * NUM_CHANNELS + 2];
			}
			r /= SCALE_FACTOR;
			b /= SCALE_FACTOR;
			g /= SCALE_FACTOR;

			poutput[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
			poutput[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
			poutput[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
		}


		dprintf("Line %u addr2 %u output %p\n", i - 1, addr2, poutput);
		//put the scaled line back
		mfc_put(output[cur_buf], addr2, LINE_SIZE / SCALE_FACTOR, tags[cur_buf], 0, 0);
		addr2 += LINE_SIZE; //line inside spu block
	}

	/* Last line */
	vv1 = v1[cur_buf];
	vv2 = v2[cur_buf];
	vv3 = v3[cur_buf];
	vv4 = v4[cur_buf];
	vv5 = v5[cur_buf];
	poutput = output[cur_buf];
	ptemp = temp[cur_buf];

	waittag(tags[cur_buf]);

	for (j = 0; j < LINE_SIZE / 16; j++){
		vv5[j] = spu_avg(spu_avg(vv1[j], vv2[j]), spu_avg(vv3[j], vv4[j]));
	}
	for (j = 0; j < img->width; j += SCALE_FACTOR){
		r = g = b = 0;
		for (k = j; k < j + SCALE_FACTOR; k++) {
			r += ptemp[k * NUM_CHANNELS + 0];
			g += ptemp[k * NUM_CHANNELS + 1];
			b += ptemp[k * NUM_CHANNELS + 2];
		}
		r /= SCALE_FACTOR;
		b /= SCALE_FACTOR;
		g /= SCALE_FACTOR;

		poutput[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
		poutput[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
		poutput[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
	}

	//put the scaled line back
	mfc_put(output[cur_buf], addr2, LINE_SIZE / SCALE_FACTOR, tags[cur_buf], 0, 0);
	waittag(tags[cur_buf]);

	dec_end = spu_read_decrementer();

	free_align(temp[0]);
	free_align(temp[1]);
	free_align(input[0]);
	free_align(input[1]);
	free_align(output[0]);
	free_align(output[1]);
}

void process_image_2lines(struct image* img){
	unsigned char *input;/* Input images lines */
	unsigned char *output;	/* Output lines */
	unsigned char *temp;	/* temporary line */
	unsigned int i, j, k, out_line, r, g, b;
	unsigned int addr1, addr2;
	int block_nr = img->block_nr;
	vector unsigned char *v1, *v2, *v3, *v4;

	const unsigned int LINE_SIZE = img->width * NUM_CHANNELS;
	const unsigned int FOUR_LINES = SCALE_FACTOR * LINE_SIZE;
	const unsigned int EIGHT_LINES = 2 * FOUR_LINES;
	vector unsigned char *vec_temp;

	input = malloc_align(2 * NUM_CHANNELS * SCALE_FACTOR * img->width, 4);
	output = malloc_align(2 * NUM_CHANNELS * img->width, 4);
	temp = malloc_align(2 * NUM_CHANNELS * img->width, 4);

	vector unsigned char * const vec1[2] = {
			(vector unsigned char *) input,
			(vector unsigned char *) (input + FOUR_LINES)
	};
	vector unsigned char * const vec2[2] = {
			(vector unsigned char *) (input + LINE_SIZE),
			(vector unsigned char *) (input + FOUR_LINES + LINE_SIZE)
	};
	vector unsigned char * const vec3[2] = {
			(vector unsigned char *) (input + 2 * LINE_SIZE),
			(vector unsigned char *) (input + FOUR_LINES + 2 * LINE_SIZE)
	};
	vector unsigned char * const vec4[2] = {
			(vector unsigned char *) (input + 3 * LINE_SIZE),
			(vector unsigned char *) (input + FOUR_LINES + 3 * LINE_SIZE)
	};
	vector unsigned char * const vec5[2] = {
			(vector unsigned char *) temp,
			(vector unsigned char *) (temp + FOUR_LINES)
	};

	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS *
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	addr1 = (unsigned int)img->src; //start of the image
	
	
	spu_write_decrementer(DECREMENTER_INIT);

	for (i = 0; i < img->height / (SCALE_FACTOR * 2); ++i) {
		/* Get 8 lines */
		mfc_get(input, addr1, EIGHT_LINES, MY_TAG, 0, 0);
		addr1 += EIGHT_LINES;
		waittag(MY_TAG);

		// Compute the scaled lines
		for (out_line = 0; out_line < 2; ++out_line) {
			v1 = vec1[out_line];
			v2 = vec2[out_line];
			v3 = vec3[out_line];
			v4 = vec4[out_line];
			vec_temp = vec5[out_line];
			for (j = 0; j < img->width * NUM_CHANNELS / 16; j++) {
				vec_temp[j] = spu_avg(spu_avg(v1[j], v2[j]), spu_avg(v3[j], v4[j]));
			}

			for (j = 0; j < img->width; j += SCALE_FACTOR) {
				r = g = b = 0;
				for (k = j; k < j + SCALE_FACTOR; k++) {
					r += temp[out_line * FOUR_LINES + k * NUM_CHANNELS + 0];
					g += temp[out_line * FOUR_LINES + k * NUM_CHANNELS + 1];
					b += temp[out_line * FOUR_LINES + k * NUM_CHANNELS + 2];
				}
				r /= SCALE_FACTOR;
				b /= SCALE_FACTOR;
				g /= SCALE_FACTOR;

				output[out_line * FOUR_LINES + j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
				output[out_line * FOUR_LINES + j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
				output[out_line * FOUR_LINES + j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
			}

			//put the scaled line back
			mfc_put(output + out_line * FOUR_LINES, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
			addr2 += LINE_SIZE; //line inside spu block
		}
		waittag(MY_TAG);
	}
	dec_end = spu_read_decrementer();
	//dprintf("%u\n", dec_end);
	free_align(temp);
	free_align(input);
	free_align(output);
}

void process_image_simple(struct image* img){
	unsigned char *input, *output, *temp;
	unsigned int addr1, addr2, i, j, k, r, g, b;
	int block_nr = img->block_nr;
	vector unsigned char *v1, *v2, *v3, *v4, *v5 ;

	input = malloc_align(NUM_CHANNELS * SCALE_FACTOR * img->width, 4);
	output = malloc_align(NUM_CHANNELS * img->width / SCALE_FACTOR, 4);
	temp = malloc_align(NUM_CHANNELS * img->width, 4);

	v1 = (vector unsigned char *) &input[0];
	v2 = (vector unsigned char *) &input[1 * img->width * NUM_CHANNELS];
	v3 = (vector unsigned char *) &input[2 * img->width * NUM_CHANNELS];
	v4 = (vector unsigned char *) &input[3 * img->width * NUM_CHANNELS];
	v5 = (vector unsigned char *) temp;

	addr2 = (unsigned int)img->dst; //start of image
	addr2 += (block_nr / NUM_IMAGES_HEIGHT) * img->width * NUM_CHANNELS *
		img->height / NUM_IMAGES_HEIGHT; //start line of spu block
	addr2 += (block_nr % NUM_IMAGES_WIDTH) * NUM_CHANNELS *
		img->width / NUM_IMAGES_WIDTH;

	spu_write_decrementer(DECREMENTER_INIT);

	for (i=0; i<img->height / SCALE_FACTOR; i++){
		//get 4 lines
		addr1 = ((unsigned int)img->src) + i * img->width * NUM_CHANNELS * SCALE_FACTOR;
		mfc_get(input, addr1, SCALE_FACTOR * img->width * NUM_CHANNELS, MY_TAG, 0, 0);
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();

		//compute the scaled line
		for (j = 0; j < img->width * NUM_CHANNELS / 16; j++){
			v5[j] = spu_avg(spu_avg(v1[j], v2[j]), spu_avg(v3[j], v4[j]));
		}
		for (j=0; j < img->width; j+=SCALE_FACTOR){
			r = g = b = 0;
			for (k = j; k < j + SCALE_FACTOR; k++) {
				r += temp[k * NUM_CHANNELS + 0];
				g += temp[k * NUM_CHANNELS + 1];
				b += temp[k * NUM_CHANNELS + 2];
			}
			r /= SCALE_FACTOR;
			b /= SCALE_FACTOR;
			g /= SCALE_FACTOR;

			output[j / SCALE_FACTOR * NUM_CHANNELS + 0] = (unsigned char) r;
			output[j / SCALE_FACTOR * NUM_CHANNELS + 1] = (unsigned char) g;
			output[j / SCALE_FACTOR * NUM_CHANNELS + 2] = (unsigned char) b;
		}

		//put the scaled line back
		mfc_put(output, addr2, img->width / SCALE_FACTOR * NUM_CHANNELS, MY_TAG, 0, 0);
		addr2 += img->width * NUM_CHANNELS; //line inside spu block
		mfc_write_tag_mask(1 << MY_TAG);
		mfc_read_tag_status_all();
	}

	dec_end = spu_read_decrementer();

	free_align(temp);
	free_align(input);
	free_align(output);
}

int main(uint64_t speid, uint64_t argp, uint64_t envp){
	unsigned int data[NUM_STREAMS];
	unsigned int num_spus = (unsigned int)argp, i, num_images;
	struct image my_image __attribute__ ((aligned(16)));
	int mode = (int)envp;

	speid = speid; //get rid of warning

	while(1){
		num_images = 0;
		for (i = 0; i < NUM_STREAMS / num_spus; i++){
			//assume NUM_STREAMS is a multiple of num_spus
			while(spu_stat_in_mbox() == 0);
			data[i] = spu_read_in_mbox();
			if (!data[i])
				return 0;
			num_images++;
		}

		for (i = 0; i < num_images; i++){
			mfc_get(&my_image, data[i], sizeof(struct image), MY_TAG, 0, 0);
			mfc_write_tag_mask(1 << MY_TAG);
			mfc_read_tag_status_all();
			switch(mode){
				default:
				case MODE_SIMPLE:
					process_image_simple(&my_image);
					break;
				case MODE_2LINES:
					process_image_2lines(&my_image);
					break;
				case MODE_DOUBLE:
					process_image_double(&my_image);
					break;
				case MODE_DMALIST:
					process_image_dmalist(&my_image);
					break;
			}
		}
		//data[0] = DONE;
		//spu_write_out_intr_mbox(data[0]);
		spu_write_out_intr_mbox(DECREMENTER_INIT - dec_end);
	}

	return 0;
}
