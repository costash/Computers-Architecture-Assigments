/* Tema 3 ASC
 * Constantin Serban-Radoi 333CA
 * Aprilie 2013
 *
 * paralel.c - Implementation on PPU
 */
#include <libspe2.h>
#include <pthread.h>
#include "paralel.h"

extern spe_program_handle_t paralel_spu;

#define MAX_SPU_THREADS 16

typedef struct pointers_t_ {
	ppu_data_t data;
	spe_context_ptr_t spe;
	spe_event_unit_t pevents;
} pointers_t;

struct image input[NUM_STREAMS] __attribute__ ((aligned(16)));
struct image big_image;
char input_path[MAX_PATH_LEN];
char output_path[MAX_PATH_LEN];
int num_frames;
int spu_threads;

spe_context_ptr_t ctxs[MAX_SPU_THREADS];
pthread_t threads[MAX_SPU_THREADS];

pointers_t pointers[MAX_SPU_THREADS] __attribute__ ((aligned(16)));

spe_event_unit_t pevents[MAX_SPU_THREADS], event_received;
spe_event_handler_ptr_t event_handler;

struct timeval t1, t2, t3, t4;
double scale_time = 0, total_time = 0;

static void wait_spu_frame(int spu_id) {
	unsigned int mbox_data;
	int nevents = spe_event_wait(event_handler, &event_received,
			ONE_EVENT, INFINITE);

	if (nevents < 0) {
		PRINT_ERR_MSG_AND_EXIT("[PPU]: Error at spe_event_wait for"\
				" SPU[%d]\n", spu_id);
	}

	dprintf("[PPU]: spe_event_wait OK for SPU[%d]."\
			" Wait returned %d\n", spu_id, nevents);

	/* Got something from SPU */
	if (event_received.events & SPE_EVENT_OUT_INTR_MBOX) {
		dprintf("[PPU]: Reading event from SPU[%d]\n", spu_id);
		spe_out_intr_mbox_read(event_received.spe, &mbox_data, 1,
				SPE_MBOX_ANY_BLOCKING);

		dprintf("[PPU]: SPU[%d] sent %d\n", spu_id, mbox_data);
	}
	else if (event_received.events & SPE_EVENT_SPE_STOPPED) {
		fprintf(stderr, "[PPU]: SPU[%d] exited and stopped\n", spu_id);
	}
	else {
		fprintf(stderr, "[PPU]: SPU[%d] unknown exit status\n", spu_id);
	}
}

static void process_frame(int frame) {
	int i = 0;
	unsigned int begin_work = BEGIN;
	char buf[MAX_PATH_LEN];

	printf("------------------------\n[PPU]: Processing Frame %d\n", frame + 1);

	/* Read input images */
	for (i = 0; i < NUM_STREAMS; ++i) {
		sprintf(buf, "%s/stream%02d/image%d.pnm", input_path,
				i + 1, frame + 1);
		read_pnm(buf, &input[i]);
	}

	big_image.width = input[0].width;
	big_image.height = input[0].height;
	alloc_image(&big_image);

	gettimeofday(&t1, NULL);
	/* Processing goes here */

	/* Inform the SPUs that they can begin to work */
	for (i = 0; i < spu_threads; i++) {
		spe_in_mbox_write(ctxs[i], (void*)&(begin_work), 1,
				SPE_MBOX_ANY_NONBLOCKING);
		dprintf("[PPU] data sent to SPU#%d = %d\n",
				i, begin_work);
	}


	/* Wait for every SPU to finish the current frame */
	for (i = 0; i < spu_threads; ++i) {
		wait_spu_frame(i);
	}

	gettimeofday(&t2, NULL);
	scale_time += GET_TIME_DELTA(t1, t2);

	/* Here goes the writing of the current frame preview */
	sprintf(buf, "%s/result%d.pnm", output_path, frame + 1);
	write_pnm(buf, &big_image);

	/* Free the image data */
	for (i = 0; i < NUM_STREAMS; ++i) {
		free_image(&input[i]);
	}
	free_image(&big_image);
}

static void do_work() {
	int i = 0;
	for (i = 0; i < num_frames; ++i) {
		process_frame(i);
	}
}

void *ppu_pthread_function(void *thread_arg) {

	pointers_t *arg = (pointers_t *) thread_arg;

	/* Run SPE context */
	unsigned int entry = SPE_DEFAULT_ENTRY;

	/* Transfer through argument the pointers */
	if (spe_context_run(arg->spe, &entry, 0, (void *)(&(arg->data)),
			(void *)sizeof(ppu_data_t), NULL) < 0) {
		fprintf(stderr, "ERROR: on SPU[%d]", arg->data.spe_id);
		perror ("Failed running context");
		exit (1);
	}


	pthread_exit(NULL);
}

int main(int argc, char** argv) {
	int i __attribute__ ((aligned(16)));


	if (argc != 4) {
		fprintf(stderr, "Usage: ./serial input_path output_path num_frames\n");
		exit(1);
	}

	gettimeofday(&t3, NULL);
	strncpy(input_path, argv[1], MAX_PATH_LEN - 1);
	strncpy(output_path, argv[2], MAX_PATH_LEN - 1);
	num_frames = atoi(argv[3]);

	if (num_frames > MAX_FRAMES)
		num_frames = MAX_FRAMES;

	spu_threads = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, -1);
	if (spu_threads > MAX_SPU_THREADS)
		spu_threads = MAX_SPU_THREADS;

	/* Create an event handler */
	event_handler = spe_event_handler_create();

	/*
	 * Create several SPE-threads to execute 'simple_spu'.
	 */
	for(i = 0; i < spu_threads; i++) {
		/* Create SPE context */
		if ((ctxs[i] = spe_context_create (SPE_EVENTS_ENABLE, NULL)) == NULL) {
			perror ("Failed creating context");
			exit (1);
		}

		/* Load SPE program into context */
		if (spe_program_load (ctxs[i], &paralel_spu)) {
			perror ("Failed loading program");
			exit (1);
		}

		/* Register events */
		pevents[i].events = SPE_EVENT_OUT_INTR_MBOX;
		pevents[i].spe = ctxs[i];
		pevents[i].data.u32 = i;
		spe_event_handler_register(event_handler, &pevents[i]);

		/* Initialize arguments for thread */
		pointers[i].data.spe_id = i;
		pointers[i].data.input = &input[i];
		pointers[i].data.big_image = &big_image;
		pointers[i].data.num_frames = num_frames;
		pointers[i].spe = ctxs[i];
		pointers[i].pevents = pevents[i];

		/* Create thread for each SPE context */
		if (pthread_create (&threads[i], NULL, &ppu_pthread_function, &pointers[i])) {
			perror ("Failed creating thread");
			exit (1);
		}
		dprintf("[PPU]: Created thread %d\n", i);
	}

	/* Here is the processing done */
	do_work();

	for (i = 0; i < spu_threads; i++) {
		/* Wait for SPU-thread to complete execution.  */
		if (pthread_join (threads[i], NULL)) {
			perror("Failed pthread_join");
			exit (1);
		}
		/* Destroy context */
		if (spe_context_destroy (ctxs[i]) != 0) {
			perror("Failed destroying context");
			exit (1);
		}
	}

	gettimeofday(&t4, NULL);
	total_time += GET_TIME_DELTA(t3, t4);

	printf("Scale time: %lf\n", scale_time);
	printf("Total time: %lf\n", total_time);

	printf("\nThe program has successfully executed.\n");
	return 0;
}
