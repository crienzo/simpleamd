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
#include <getopt.h>

int debug = 0;
int summarize = 0;
int amd_silence_ms = 2000;
int amd_machine_ms = 900;
int vad_energy_threshold = 200;
int vad_voice_ms = 20;
int vad_silence_ms = 500;

static const char *result_string[4] = { "unknown", "human", "machine", "dead-air" };
enum amd_test_result {
	RESULT_UNKNOWN = 0,
	RESULT_HUMAN,
	RESULT_MACHINE,
	RESULT_DEAD_AIR
};

struct amd_test_stats {
	int humans;
	int humans_detected_as_machine;
	int humans_detected_as_unknown;
	int humans_detected_as_dead_air;
	int machines;
	int machines_detected_as_human;
	int machines_detected_as_unknown;
	int machines_detected_as_dead_air;
};

static void amd_logger(samd_log_level_t level, void *user_log_data, const char *file, int line, const char *message)
{
	printf("%s:%d\t%s", file, line, message);
}

static void amd_event_handler(samd_event_t event, uint32_t time_ms, void *user_event_data)
{
	enum amd_test_result *result = (enum amd_test_result *)user_event_data;
	if (event == SAMD_MACHINE_VOICE || event == SAMD_MACHINE_SILENCE) {
		*result = RESULT_MACHINE;
	} else if (event == SAMD_HUMAN_SILENCE || event == SAMD_HUMAN_VOICE) {
		*result = RESULT_HUMAN;
	} else if (event == SAMD_DEAD_AIR) {
		*result = RESULT_DEAD_AIR;
	}
}

static enum amd_test_result get_expected_result_from_audio_file_name(const char *raw_audio_file_name)
{
	if (strcasestr(raw_audio_file_name, "machine")) {
		return RESULT_MACHINE;
	} else if (strcasestr(raw_audio_file_name, "human") || strcasestr(raw_audio_file_name, "person")) {
		return RESULT_HUMAN;
	}
	return RESULT_UNKNOWN;
}

static enum amd_test_result analyze_file(struct amd_test_stats *test_stats, const char *raw_audio_file_name, enum amd_test_result expected_result)
{
	samd_vad_t *vad = NULL;
	samd_t *amd = NULL;
	FILE *raw_audio_file;
	int16_t samples[80] = { 0 };
	enum amd_test_result result = RESULT_UNKNOWN;

	samd_vad_init(&vad);
	if (!vad) {
		fprintf(stderr, "Failed to initialize AMD VAD\n");
		exit(EXIT_FAILURE);
	}
	samd_vad_set_energy_threshold(vad, vad_energy_threshold); /* energy above this threshold is considered voice */
	samd_vad_set_silence_ms(vad, vad_silence_ms); /* how long to wait for end of voice */
	samd_vad_set_voice_ms(vad, vad_voice_ms); /* how long to wait for start of voice */
	if (debug) {
		samd_vad_set_log_handler(vad, amd_logger, NULL);
	}

	samd_init(&amd, vad);
	if (!amd) {
		fprintf(stderr, "Failed to initialize AMD\n");
		exit(EXIT_FAILURE);
	}
	samd_set_machine_ms(amd, amd_machine_ms); /* voice longer than this is classified machine */
	samd_set_silence_start_ms(amd, amd_silence_ms); /* maximum duration of initial silence to allow */
	samd_set_event_handler(amd, amd_event_handler, &result);
	if (debug) {
		samd_set_log_handler(amd, amd_logger, NULL);
	}

	raw_audio_file = fopen(raw_audio_file_name, "rb");
	if (!raw_audio_file) {
		perror(raw_audio_file_name);
		exit(EXIT_FAILURE);
	}

	while (!feof(raw_audio_file) && !ferror(raw_audio_file) && result == RESULT_UNKNOWN) {
		size_t num_samples = fread(samples, sizeof(int16_t), 80, raw_audio_file);
		if (num_samples > 0) {
			samd_process_buffer(amd, samples, num_samples);
		}
	}

	fclose(raw_audio_file);
	samd_destroy(&amd);
	samd_vad_destroy(&vad);

	if (expected_result == RESULT_MACHINE) {
		test_stats->machines++;
		switch (result) {
			case RESULT_UNKNOWN: test_stats->machines_detected_as_unknown++; break;
			case RESULT_HUMAN: test_stats->machines_detected_as_human++; break;
			case RESULT_DEAD_AIR: test_stats->machines_detected_as_dead_air++; break;
			default: break;
		}
	} else if (expected_result == RESULT_HUMAN) {
		test_stats->humans++;
		switch (result) {
			case RESULT_UNKNOWN: test_stats->humans_detected_as_unknown++; break;
			case RESULT_MACHINE: test_stats->humans_detected_as_machine++; break;
			case RESULT_DEAD_AIR: test_stats->humans_detected_as_dead_air++; break;
			default: break;
		}
	}

	printf("%s,%s\n", raw_audio_file_name, result_string[result]);

	return result;
}

#define USAGE "simpleamd <-f <raw audio file>|-l <list file>>"
#define HELP USAGE"\n" \
	"\t-f <raw audio file> 8000 kHz RAW LPCM input file\n" \
	"\t-l <list file> text file listing raw audio files to test\n" \
	"\t-e <vad energy> energy threshold (default 200)\n" \
	"\t-v <vad voice ms> consecutive speech to trigger start of voice (default 20)\n" \
	"\t-s <vad silence ms> consecutive silence to trigger start of silence (default 500)\n" \
	"\t-m <amd machine ms> voice longer than this time is classified as machine (default 900)\n" \
	"\t-w <amd wait for voice ms> how long to wait for voice to begin (default 2000)\n" \
	"\t-d Enable debug logging\n" \
	"\t-r Summarize results\n"

int main(int argc, char **argv)
{
	struct amd_test_stats test_stats = { 0 };
	char *list_file_name = NULL;
	char *raw_audio_file_name = NULL;
	int opt;

	while ((opt = getopt(argc, argv, "f:l:e:v:s:m:w:dr")) != -1) {
		switch (opt) {
			case 'f':
				raw_audio_file_name = strdup(optarg);
				break;
			case 'l':
				list_file_name = strdup(optarg);
				break;
			case 'e': {
				int val = atoi(optarg);
				if (val > 0 && val < 32767) {
					vad_energy_threshold = val;
				} else {
					fprintf(stderr, "option -e must be > 0 and < 32767\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'v': {
				int val = atoi(optarg);
				if (val > 0) {
					vad_voice_ms = val;
				} else {
					fprintf(stderr, "option -v must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 's': {
				int val = atoi(optarg);
				if (val > 0) {
					vad_silence_ms = val;
				} else {
					fprintf(stderr, "option -s must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'm': {
				int val = atoi(optarg);
				if (val > 0) {
					amd_machine_ms = val;
				} else {
					fprintf(stderr, "option -m must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'w': {
				int val = atoi(optarg);
				if (val > 0) {
					amd_silence_ms = val;
				} else {
					fprintf(stderr, "option -w must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'd':
				debug = 1;
				break;
			case 'r':
				summarize = 1;
				break;
			default:
				printf("%s", HELP);
				exit(EXIT_SUCCESS);
				break;
		}
	}

	/* list file and raw audio file are mutually exclusive */
	if ((!list_file_name && !raw_audio_file_name) || (list_file_name && raw_audio_file_name)) {
		fprintf(stderr, USAGE"\n");
		exit(EXIT_FAILURE);
	}

	/* analyze the files */
	if (list_file_name) {
		char raw_audio_file_buf[1024];
		FILE *list_file = fopen(list_file_name, "r");
		if (!list_file) {
			perror(list_file_name);
			exit(EXIT_FAILURE);
		}
		while (!feof(list_file) && !ferror(list_file) && fgets(raw_audio_file_buf, sizeof(raw_audio_file_buf), list_file)) {
			char *newline;
			if ((newline = strrchr(raw_audio_file_buf, '\n'))) {
				*newline = '\0';
			}
			analyze_file(&test_stats, raw_audio_file_buf, get_expected_result_from_audio_file_name(raw_audio_file_buf));
		}
		fclose(list_file);
	} else {
		analyze_file(&test_stats, raw_audio_file_name, get_expected_result_from_audio_file_name(raw_audio_file_name));
	}

	/* output final stats */
	if (summarize && test_stats.humans + test_stats.machines > 0) {
		printf("\n*** SUMMARY ***\n");
		printf("expected,machines,humans,dead-air,unknown,accuracy\n");
		if (test_stats.humans > 0) {
			/* accuracy counts dead air as human */
			int detected_humans = test_stats.humans -
				test_stats.humans_detected_as_machine - test_stats.humans_detected_as_unknown;
			double human_detection_accuracy = ((double)detected_humans / (double)test_stats.humans) * 100.0;
			printf("human,%d,%d,%d,%d,%0.2f\n",
				test_stats.humans_detected_as_machine,
				detected_humans - test_stats.humans_detected_as_dead_air,
				test_stats.humans_detected_as_dead_air,
				test_stats.humans_detected_as_unknown,
				human_detection_accuracy);
		}

		if (test_stats.machines > 0) {
			int detected_machines = test_stats.machines - test_stats.machines_detected_as_dead_air -
				test_stats.machines_detected_as_human - test_stats.machines_detected_as_unknown;
			double machine_detection_accuracy = ((double)detected_machines / (double)test_stats.machines) * 100.0;
			printf("machine,%d,%d,%d,%d,%0.2f\n",
				detected_machines,
				test_stats.machines_detected_as_human,
				test_stats.machines_detected_as_dead_air,
				test_stats.machines_detected_as_unknown,
				machine_detection_accuracy);
		}
	}

	return EXIT_SUCCESS;
}
