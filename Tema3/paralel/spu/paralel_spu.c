/* Tema 3 ASC
 * Constantin Serban-Radoi 333CA
 * Aprilie 2013
 *
 * paralel_spu.c - SPU implementation
 */
#include <stdio.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include "../ppu/paralel.h"

#define waittag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
uint32_t tag_id;

/* Waits for message from PPU to begin work on the current frame */
static void wait_for_begin(uint32_t *mbox_message) {
	do {
		while (spu_stat_in_mbox() <= 0)
			;
		*mbox_message = spu_read_in_mbox();

	} while (*mbox_message != BEGIN);
}

/* Sends a message to PPU that the current frame has been processed */
static void send_frame_done(ppu_data_t ppu_data, int frame_id) {
	uint32_t mbox_message = SPU_FRAME_DONE;

	while (spu_stat_out_intr_mbox() <= 0)
		;
	spu_write_out_intr_mbox(mbox_message);
	dprintf("SPU[%d]: Finished frame %d. Informed PPU\n",
		ppu_data.spe_id, frame_id + 1);
	/* Suppress warnings of unused */
	ppu_data.spe_id = ppu_data.spe_id;
	frame_id = frame_id;
}

/* Make average on lines */
static void compute_lines_average(struct image *img_chunk, uint32_t buf_line_sz) {
	volatile vector unsigned char* v_line_accum;
	volatile vector unsigned char* v_line1;
	volatile vector unsigned char* v_line2;
	unsigned int j, k;

	v_line_accum = (vector unsigned char*)(img_chunk->data);
	/* Make average on lines (accum[0] = (l1[0] + l2[0]) / 2, etc. */
	for (j = 0; j < 2; ++j) {

		v_line1 = (vector unsigned char*)(img_chunk->data + (j * 2) * buf_line_sz);
		v_line2 = (vector unsigned char*)(img_chunk->data + (j * 2 + 1) * buf_line_sz);
		for (k = 0; k < buf_line_sz / 16; ++k) {
			v_line1[k] = spu_avg(v_line1[k], v_line2[k]);
		}
	}
	v_line2 = (vector unsigned char*)(img_chunk->data);
	for (k = 0; k < buf_line_sz / 4; ++k) {
		v_line_accum[k] = spu_avg(v_line1[k], v_line2[k]);
	}
}

/* Make average on columns */
static void compute_columns_average(struct image *img_chunk,
		struct image *img_scaled_line) {
	unsigned int j, k;

	for (j = 0; j < img_scaled_line->width; ++j) {
		unsigned int rval = 0, gval = 0, bval = 0;
		for (k = j * SCALE_FACTOR; k < (j + 1) * SCALE_FACTOR; ++k) {
			rval += img_chunk->data[k * NUM_CHANNELS];
			gval += img_chunk->data[k * NUM_CHANNELS + 1];
			bval += img_chunk->data[k * NUM_CHANNELS + 2];
		}
		img_scaled_line->data[j * NUM_CHANNELS] = rval / SCALE_FACTOR;
		img_scaled_line->data[j * NUM_CHANNELS + 1] = gval / SCALE_FACTOR;
		img_scaled_line->data[j * NUM_CHANNELS + 2] = bval / SCALE_FACTOR;
	}
}

/* Stores the currently processed line in big_image from PPU */
static void store_line(struct image *img_scaled_line, ppu_data_t ppu_data,
		struct image *big_image, unsigned int chunk_no) {
	/* The line must be transferred directly to big_image on PPU
	 * This is done by several DMAs, because the result will not be
	 * aligned to 16B. 624 / 4 cannot be divided by 16, therefore,
	 * there will be several DMA transfers to accomplish this.
	 *
	 * The first thumbnail will be split into DMA transfers of size
	 * 16*x and 4 Bytes
	 * The second one: 4 Bytes then 8 Bytes then 16 * (x - 1) Bytes
	 *		then 8 Bytes
	 * The third one: 8 Bytes then 16 * (x - 1) Bytes then 8 Bytes
	 *		then 4 Bytes
	 * The fourth one: 4 Bytes then 16 * x Bytes
	 * Where 16 * x + 4 represents the size of the line
	 */
	uint32_t tile_row_sz = big_image->width * (big_image->height / 4) * 3;
	uint32_t base_dma_addr = (uint32_t)big_image->data +
			(ppu_data.spe_id / 4) * tile_row_sz +
			big_image->width * 3 * chunk_no +
			(ppu_data.spe_id % 4) * img_scaled_line->width * 3;
	uint32_t base_local_addr = (uint32_t)img_scaled_line->data;
	unsigned int first_size;
	unsigned int second_size;
	unsigned int third_size;
	unsigned int fourth_size;
	int two_transfers = 0;	/* 1 if  the first and fourth thumbnails
							 * 0 for the second and third thumbnails
							 */
	switch (ppu_data.spe_id % 4) {
		case 0:
			/* First thumbnail */
			first_size = (img_scaled_line->width * 3) / 16 * 16;
			fourth_size = (img_scaled_line->width * 3) % 16;
			two_transfers = 1;
			break;
		case 1:
			/* Second thumbnail */
			first_size = 4;
			second_size = 8;
			third_size = ((img_scaled_line->width * 3) / 16 - 1) * 16;
			fourth_size = 8;
			break;
		case 2:
			/* Second thumbnail */
			first_size = 8;
			second_size = ((img_scaled_line->width * 3) / 16 - 1) * 16;
			third_size = 8;
			fourth_size = 4;
			break;
		case 3:
			/* Fourth thumbnail */
			first_size = (img_scaled_line->width * 3) % 16;
			fourth_size = (img_scaled_line->width * 3)/ 16 * 16;
			two_transfers = 1;
			break;
		default:
			break;
	}

	dprintf("SPU[%d]-chk[%d] base_dma_addr=%p base_local_addr=%p first_size=%u"\
			" second_size=%u third_size=%u fourth_size=%u two_tr=%d\n",
			ppu_data.spe_id, i, (void *)base_dma_addr,
			(void *)base_local_addr, first_size,
			second_size, third_size, fourth_size, two_transfers);
	 /* Transfer first block of the line */
	mfc_put((void *)(base_local_addr), base_dma_addr,
			(uint32_t)first_size, tag_id, 0, 0);

#ifdef DEBUG
	waittag(tag_id);
#endif

	base_dma_addr += first_size;
	base_local_addr += first_size;

	dprintf("SPU[%d]-chk[%d] sent second_dma_addr=%p second_local_addr = %p\n",
			ppu_data.spe_id, i, (void *)base_dma_addr,
			(void *)base_local_addr);

	/* Send the second and the third blocks of the line only if the current
	 * thumbnail is in the middle of the resulting image
	 */
	if (two_transfers == 0) {
		mfc_put((void *)(base_local_addr), base_dma_addr,
				(uint32_t)second_size, tag_id, 0, 0);

		base_dma_addr += second_size;
		base_local_addr += second_size;
		mfc_put((void *)(base_local_addr), base_dma_addr,
				(uint32_t)third_size, tag_id, 0, 0);

		base_dma_addr += third_size;
		base_local_addr += third_size;
	}

	/* Send the last block of the line */
	mfc_put((void *)(base_local_addr), base_dma_addr,
			(uint32_t)fourth_size, tag_id, 0, 0);

	waittag(tag_id);

	dprintf("SPU[%d]-chk[%d] sent fourth_dma_addr=%p second_local_addr = %p\n",
			ppu_data.spe_id, i, (void *)base_dma_addr, (void *)base_local_addr);
}

/* Does the actual processing of the frame */
static void do_work(ppu_data_t ppu_data) {
	struct image input;
	struct image big_image;

	dprintf("SPU[%d] ppu_data.input:%p ppu_big_img:%p sizeof(struct image):%lu\n",
		ppu_data.spe_id, (void *)ppu_data.input,
		(void *)ppu_data.big_image, sizeof(struct image));

	/* Get input image and big_image details */
	mfc_get((void *)(&input), (uint32_t)(ppu_data.input),
			(uint32_t)(sizeof(struct image)), tag_id, 0, 0);
	mfc_get((void *)(&big_image), (uint32_t)(ppu_data.big_image),
			(uint32_t)(sizeof(struct image)), tag_id, 0, 0);

	waittag(tag_id);
	dprintf("SPU[%d] got structs\n"\
			"input.width=%u\tinput.height=%u\n"\
			"big_image.width=%u\tbig_image.height=%u\n"\
			"input.data=%p\tbig_image.data=%p\n",
			ppu_data.spe_id, input.width, input.height, big_image.width,
			big_image.height, (void *)input.data, (void *)big_image.data);

	struct image img_chunk;
	unsigned int buf_line_sz = input.width * NUM_CHANNELS;
	int transfer_sz = 4 * buf_line_sz;

	img_chunk.width = input.width;
	img_chunk.height = 4;
	alloc_image(&img_chunk);

	struct image img_scaled_line;
	img_scaled_line.width = input.width / SCALE_FACTOR;
	img_scaled_line.height = 1;

	/* Hack for memory align of local image data to have the same 4 bits in its
	 * address as the remote corresponding address in PPU
	 */
	int left_padding = (ppu_data.spe_id % 4) * 4;
	unsigned char* addr_to_free = malloc_align(NUM_CHANNELS * 3 * sizeof(char) +
												left_padding, 4);

	img_scaled_line.data = addr_to_free + left_padding;

	unsigned int i;
	/* Process 4 lines from the initial image at a time */
	for (i = 0; i < input.height / img_chunk.height; ++i) {

		/* Get the image chunk from PPU through DMA transfer */
		dprintf("SPU[%d] getting image_chunk %d of size %d\n",
				ppu_data.spe_id, i, transfer_sz);

		dprintf("SPU[%d] input.data=%p img_chunk.data=%p "\
				"start_addr=%p\n", ppu_data.spe_id, (void *)input.data,
				(void *)img_chunk.data, (void *)((uint32_t)(input.data) + i * transfer_sz));

		mfc_get((void *)(img_chunk.data), (uint32_t)(input.data) + i * transfer_sz,
				(uint32_t)(transfer_sz), tag_id, 0, 0);

		waittag(tag_id);
		dprintf("SPU[%d] got image_chunk %d\n", ppu_data.spe_id, i);

		compute_lines_average(&img_chunk, buf_line_sz);

		/* Make average for column. avg = (c0.r + c1.r) / 2 etc*/
		compute_columns_average(&img_chunk, &img_scaled_line);

		store_line(&img_scaled_line, ppu_data, &big_image, i);
	}

	free_image(&img_chunk);
	free_align(addr_to_free);
}

static void process_frame(ppu_data_t ppu_data, int frame_id) {
	uint32_t mbox_message = NOT_READY;

	wait_for_begin(&mbox_message);

	dprintf("SPU[%d]: Got %u. Processing frame %d\n",
			ppu_data.spe_id, mbox_message, frame_id + 1);

	/* Here is the processing */
	do_work(ppu_data);

	dprintf("SPU[%d]: Processing frame %d finished. Informing ppu.\n",
			ppu_data.spe_id, frame_id + 1);

	/* Processing current frame is finished. Must inform PPU */
	send_frame_done(ppu_data, frame_id);
}

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp)
{
	int i = 0;
	ppu_data_t ppu_data __attribute__ ((aligned(16)));

	tag_id = mfc_tag_reserve();
	if (tag_id == MFC_TAG_INVALID){
		printf("SPU: ERROR can't allocate tag ID\n");
		return -1;
	}

	/* Obtin prin DMA structura cu pointeri, nr de frame-uri si spe_id */
	dprintf("SPU: am intrat in spu %llx %lu %llx\n",
			speid, sizeof(ppu_data_t), envp);
	mfc_get((void*)&ppu_data, argp, (uint32_t)envp, tag_id, 0, 0);
	waittag(tag_id);

	dprintf("SPU: speid:%llx got struct\n", speid);
	dprintf("SPU: speid:%llx id:%02d input:%p big_img:%p num_frms:%d\n",
			speid, ppu_data.spe_id, ppu_data.input, ppu_data.big_image,
			ppu_data.num_frames);
	speid = speid;

	/* Frame processing goes here */
	for (i = 0; i < ppu_data.num_frames; ++i) {
		process_frame(ppu_data, i);
	}

	return 0;
}

