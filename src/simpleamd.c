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
int amd_machine_ms = 1100;
double vad_energy_threshold = 130.0;
int vad_voice_ms = 20;
int vad_silence_ms = 500;
int vad_sample_rate = 8000;
int vad_channels = 1;
int vad_initial_adjust_ms = 100;
int vad_voice_adjust_ms = 50;
int vad_adjust_limit = 3;

static const char *result_string[4] = { "unknown", "human", "machine", "no-voice" };
enum amd_test_result {
	RESULT_UNKNOWN = 0,
	RESULT_HUMAN,
	RESULT_MACHINE,
	RESULT_NO_VOICE
};

struct amd_test_stats {
	int humans;
	int humans_detected_as_machine;
	int humans_detected_as_unknown;
	int humans_detected_as_no_voice;
	int machines;
	int machines_detected_as_human;
	int machines_detected_as_unknown;
	int machines_detected_as_no_voice;
};

static void amd_logger(samd_log_level_t level, void *user_log_data, const char *file, int line, const char *message)
{
	printf("%s\t\t%s:%d\t%s", (char *)user_log_data, file, line, message);
}

static void amd_event_handler(samd_event_t event, uint32_t time_ms, void *user_event_data)
{
	enum amd_test_result *result = (enum amd_test_result *)user_event_data;
	if (event == SAMD_MACHINE_VOICE || event == SAMD_MACHINE_SILENCE || event == SAMD_MACHINE_BEEP) {
		*result = RESULT_MACHINE;
	} else if (event == SAMD_HUMAN_SILENCE || event == SAMD_HUMAN_VOICE) {
		*result = RESULT_HUMAN;
	} else if (event == SAMD_NO_VOICE) {
		*result = RESULT_NO_VOICE;
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

	/* create AMD */
	samd_init(&amd);
	if (!amd) {
		fprintf(stderr, "Failed to initialize AMD\n");
		exit(EXIT_FAILURE);
	}
	samd_set_sample_rate(amd, vad_sample_rate);
	samd_set_machine_ms(amd, amd_machine_ms); /* voice longer than this is classified machine */
	samd_set_silence_start_ms(amd, amd_silence_ms); /* maximum duration of initial silence to allow */
	samd_set_event_handler(amd, amd_event_handler, &result);
	if (debug) {
		samd_set_log_handler(amd, amd_logger, (void *)raw_audio_file_name);
	}

	/* configure VAD for AMD */
	vad = samd_get_vad(amd);
	samd_vad_set_energy_threshold(vad, vad_energy_threshold); /* energy above this threshold is considered voice */
	samd_vad_set_silence_ms(vad, vad_silence_ms); /* how long to wait for end of voice */
	samd_vad_set_voice_ms(vad, vad_voice_ms); /* how long to wait for start of voice */
	samd_vad_set_initial_adjust_ms(vad, vad_initial_adjust_ms); /* time to adjust energy threshold relative to start */
	samd_vad_set_voice_adjust_ms(vad, vad_voice_adjust_ms); /* time to adjust energy threshold relative to start of voice */
	samd_vad_set_adjust_limit(vad, vad_adjust_limit);

	raw_audio_file = fopen(raw_audio_file_name, "rb");
	if (!raw_audio_file) {
		perror(raw_audio_file_name);
		exit(EXIT_FAILURE);
	}

	while (!feof(raw_audio_file) && !ferror(raw_audio_file) && result == RESULT_UNKNOWN) {
		size_t num_samples = fread(samples, sizeof(int16_t), 80, raw_audio_file);
		if (num_samples > 0) {
			samd_process_buffer(amd, samples, num_samples, vad_channels);
		}
	}

	fclose(raw_audio_file);
	samd_destroy(&amd);

	if (expected_result == RESULT_MACHINE) {
		test_stats->machines++;
		switch (result) {
			case RESULT_UNKNOWN: test_stats->machines_detected_as_unknown++; break;
			case RESULT_HUMAN: test_stats->machines_detected_as_human++; break;
			case RESULT_NO_VOICE: test_stats->machines_detected_as_no_voice++; break;
			default: break;
		}
	} else if (expected_result == RESULT_HUMAN) {
		test_stats->humans++;
		switch (result) {
			case RESULT_UNKNOWN: test_stats->humans_detected_as_unknown++; break;
			case RESULT_MACHINE: test_stats->humans_detected_as_machine++; break;
			case RESULT_NO_VOICE: test_stats->humans_detected_as_no_voice++; break;
			default: break;
		}
	}

	printf("%s,%s\n", raw_audio_file_name, result_string[result]);

	return result;
}

#define USAGE "simpleamd <-f <raw audio file>|-l <list file>>"
#define HELP USAGE"\n" \
	"\t-f <raw audio file> RAW LPCM input file\n" \
	"\t-l <list file> Text file listing raw audio files to test\n" \
	"\t-e <vad energy> Energy threshold (default 130)\n" \
	"\t-v <vad voice ms> Consecutive speech to trigger start of voice (default 20)\n" \
	"\t-s <vad silence ms> Consecutive silence to trigger start of silence (default 500)\n" \
	"\t-i <vad initial adjust ms> Time to measure background environment before starting VAD.  Disable with 0. (default 100)\n" \
	"\t-r <vad sample rate> Sample rate of input audio (default 8000)\n" \
	"\t-c <vad channels> Number of channels per sample (default 1)\n" \
	"\t-n <vad voice adjust ms> Time relative to start of initial utterance for voice adjustment.  Disable with 0. (default 50)\n" \
	"\t-a <vad adjust threshold> maximum factor to adjust energy threshold relative to current threshold.  (default 3)\n" \
	"\t-m <amd machine ms> Voice longer than this time is classified as machine (default 1100)\n" \
	"\t-w <amd wait for voice ms> How long to wait for voice to begin (default 2000)\n" \
	"\t-d Enable debug logging\n" \
	"\t-R Summarize results\n"

int main(int argc, char **argv)
{
	struct amd_test_stats test_stats = { 0 };
	char *list_file_name = NULL;
	char *raw_audio_file_name = NULL;
	int opt;

	while ((opt = getopt(argc, argv, "a:f:l:e:v:s:i:m:w:c:r:n:dR")) != -1) {
		switch (opt) {
			case 'f':
				raw_audio_file_name = strdup(optarg);
				break;
			case 'l':
				list_file_name = strdup(optarg);
				break;
			case 'e': {
				double val = atof(optarg);
				if (val > 0.0 && val < 32767.0) {
					vad_energy_threshold = val;
				} else {
					fprintf(stderr, "option -e (vad energy threshold) must be > 0 and < 32767\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'v': {
				int val = atoi(optarg);
				if (val > 0) {
					vad_voice_ms = val;
				} else {
					fprintf(stderr, "option -v (vad voice ms) must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 's': {
				int val = atoi(optarg);
				if (val > 0) {
					vad_silence_ms = val;
				} else {
					fprintf(stderr, "option -s (vad silence ms) must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'i': {
				int val = atoi(optarg);
				if (val >= 0) {
					vad_initial_adjust_ms = val;
				} else {
					fprintf(stderr, "option -i (vad initial adjust ms) must be >= 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'n': {
				int val = atoi(optarg);
				if (val >= 0) {
					vad_voice_adjust_ms = val;
				} else {
					fprintf(stderr, "option -n (vad voice adjust ms) must be >= 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'a': {
				int val = atoi(optarg);
				if (val > 0) {
					vad_adjust_limit = val;
				} else {
					fprintf(stderr, "option -a (vad adjust limit) must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'r': {
				int val = atoi(optarg);
				if (val >= 8000) {
					vad_sample_rate = val;
				} else {
					fprintf(stderr, "option -r (vad sample rate) must be >= 8000\n");
					exit(EXIT_FAILURE);
				}
			}
			case 'c': {
				int val = atoi(optarg);
				if (val > 0) {
					vad_channels = val;
				} else {
					fprintf(stderr, "option -c (vad channels) must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'm': {
				int val = atoi(optarg);
				if (val > 0) {
					amd_machine_ms = val;
				} else {
					fprintf(stderr, "option -m (amd machine ms) must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'w': {
				int val = atoi(optarg);
				if (val > 0) {
					amd_silence_ms = val;
				} else {
					fprintf(stderr, "option -w (amd wait for voice ms) must be > 0\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
			case 'd':
				debug = 1;
				break;
			case 'R':
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
			if (raw_audio_file_buf[0] != '\0' && raw_audio_file_buf[0] != '#') {
				analyze_file(&test_stats, raw_audio_file_buf, get_expected_result_from_audio_file_name(raw_audio_file_buf));
			}
		}
		fclose(list_file);
	} else {
		analyze_file(&test_stats, raw_audio_file_name, get_expected_result_from_audio_file_name(raw_audio_file_name));
	}

	/* output final stats */
	if (summarize && test_stats.humans + test_stats.machines > 0) {
		int total = 0;
		int correctly_detected_total = 0;

		printf("\n*** SUMMARY ***\n");
		printf("expected,machines,humans,dead-air,unknown,accuracy\n");
		if (test_stats.humans > 0) {
			/* accuracy counts dead air as human */
			int detected_humans = test_stats.humans -
				test_stats.humans_detected_as_machine - test_stats.humans_detected_as_unknown;
			double human_detection_accuracy = ((double)detected_humans / (double)test_stats.humans) * 100.0;
			printf("human,%d,%d,%d,%d,%0.2f\n",
				test_stats.humans_detected_as_machine,
				detected_humans - test_stats.humans_detected_as_no_voice,
				test_stats.humans_detected_as_no_voice,
				test_stats.humans_detected_as_unknown,
				human_detection_accuracy);

			total += test_stats.humans;
			correctly_detected_total += detected_humans;
		}

		if (test_stats.machines > 0) {
			int detected_machines = test_stats.machines - test_stats.machines_detected_as_no_voice -
				test_stats.machines_detected_as_human - test_stats.machines_detected_as_unknown;
			double machine_detection_accuracy = ((double)detected_machines / (double)test_stats.machines) * 100.0;
			printf("machine,%d,%d,%d,%d,%0.2f\n",
				detected_machines,
				test_stats.machines_detected_as_human,
				test_stats.machines_detected_as_no_voice,
				test_stats.machines_detected_as_unknown,
				machine_detection_accuracy);

			total += test_stats.machines;
			correctly_detected_total += detected_machines;
		}

		printf("\noverall accuracy = (%d/%d) * 100.0 = %f\n", correctly_detected_total, total,
			((double)correctly_detected_total / (double)total) * 100.0);
	}

	return EXIT_SUCCESS;
}
