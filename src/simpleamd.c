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

static void amd_logger(samd_log_level_t level, void *user_log_data, const char *file, int line, const char *message)
{
	printf("%s:%d\t%s", file, line, message);
}

static void amd_event_handler(samd_event_t event, uint32_t time_ms, void *user_event_data)
{
	//printf("%d %s\n", time_ms, samd_event_to_string(event));
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
	samd_vad_set_log_handler(vad, amd_logger, NULL);

	samd_init(&amd, vad);
	if (!amd) {
		fprintf(stderr, "Failed to initialize AMD\n");
		exit(EXIT_FAILURE);
	}
	samd_set_event_handler(amd, amd_event_handler, NULL);
	samd_set_log_handler(amd, amd_logger, NULL);

	raw_audio_file = fopen(argv[1], "rb");
	if (!raw_audio_file) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	while (!feof(raw_audio_file) && !ferror(raw_audio_file)) {
		size_t num_samples = fread(samples, sizeof(int16_t), 4098, raw_audio_file);
		if (num_samples > 0) {
			samd_process_buffer(amd, samples, num_samples);
		}
	}

	fclose(raw_audio_file);
	samd_destroy(amd);
	samd_destroy(vad);

	return EXIT_SUCCESS;
}

