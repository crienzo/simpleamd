/* 
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */
#include <simpleamd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define DEBUG_LOGS 1

#define UNKNOWN "unknown"
#define HUMAN "human"
#define MACHINE "machine"

int have_result = 0;
const char *result = UNKNOWN;

static void amd_logger(samd_log_level_t level, void *user_log_data, const char *file, int line, const char *message)
{
	printf("%s:%d\t%s", file, line, message);
}

static void amd_event_handler(samd_event_t event, uint32_t time_ms, void *user_event_data)
{
	if (event == SAMD_MACHINE_VOICE || event == SAMD_MACHINE_SILENCE) {
		result = MACHINE;
	} else if (event == SAMD_HUMAN_SILENCE || event == SAMD_HUMAN_VOICE) {
		result = HUMAN;
	}
	have_result = 1;
}

int main(int argc, char **argv)
{
	samd_vad_t *vad = NULL;
	samd_t *amd = NULL;
	char *raw_audio_file_name;
	FILE *raw_audio_file;
	int16_t samples[4098] = { 0 };

	if (argc != 2) {
		fprintf(stderr, "Usage: simpleamd <raw 8000 kHz filename>\n");
		exit(EXIT_FAILURE);
	}

	samd_vad_init(&vad);
	if (!vad) {
		fprintf(stderr, "Failed to initialize AMD VAD\n");
		exit(EXIT_FAILURE);
	}
	samd_vad_set_silence_ms(vad, 500); /* wait 1/2 second for end of voice */
#if DEBUG_LOGS
	samd_vad_set_log_handler(vad, amd_logger, NULL);
#endif

	samd_init(&amd, vad);
	if (!amd) {
		fprintf(stderr, "Failed to initialize AMD\n");
		exit(EXIT_FAILURE);
	}
	samd_set_event_handler(amd, amd_event_handler, NULL);
#if DEBUG_LOGS
	samd_set_log_handler(amd, amd_logger, NULL);
#endif

	raw_audio_file = fopen(argv[1], "rb");
	if (!raw_audio_file) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	while (!feof(raw_audio_file) && !ferror(raw_audio_file) && !have_result) {
		size_t num_samples = fread(samples, sizeof(int16_t), 80, raw_audio_file);
		if (num_samples > 0) {
			samd_process_buffer(amd, samples, num_samples);
		}
	}

	fclose(raw_audio_file);
	samd_destroy(&amd);
	samd_vad_destroy(&vad);

	printf("%s,%s\n", argv[1], result);

	return EXIT_SUCCESS;
}
