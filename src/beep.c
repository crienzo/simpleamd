/*
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <math.h>
#include "samd_private.h"

#define BEEP_DEFAULT_ENERGY_THRESHOLD 130.0

/**
 * NO-OP log handler
 */
static void null_log_handler(samd_log_level_t level, void *user_data, const char *file, int line, const char *message)
{
	/* ignore */
}

/**
 * NO-OP beep event handler
 */
static void null_event_handler(uint32_t time_ms, void *user_data)
{
	/* ignore */
}

/**
 * Set optional logger
 * @param vad
 * @param log_handler
 */
void samd_beep_set_log_handler(samd_beep_t *beep, samd_log_fn log_handler, void *user_log_data)
{
	beep->user_log_data = user_log_data;
	beep->log_handler = log_handler;
}

/**
 * Set event handler
 * @param vad
 * @param event_handler
 */
void samd_beep_set_event_handler(samd_beep_t *beep, samd_beep_event_fn event_handler, void *user_event_data)
{
	beep->user_event_data = user_event_data;
	beep->event_handler = event_handler;
}

/**
 * Set the sample rate of the audio
 * @param beep
 * @param sample_rate
 */
void samd_beep_set_sample_rate(samd_beep_t *beep, uint32_t sample_rate)
{
	samd_frame_analyzer_set_sample_rate(beep->analyzer, sample_rate);
}

/**
 * Handle the next frame of processed audio
 * @param analzyer the frame analyzer
 * @param user_data this beep detector
 */
void samd_beep_process_frame(samd_frame_analyzer_t *analyzer, void *user_data, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	samd_beep_t *beep = (samd_beep_t *)user_data;

	/* TODO */
}

/**
 * Process the next buffer of samples
 * @param vad
 * @param samples
 * @param num_samples
 * @param channels
 */
void samd_beep_process_buffer(samd_beep_t *beep, int16_t *samples, uint32_t num_samples, uint32_t channels)
{
	samd_frame_analyzer_process_buffer(beep->analyzer, samples, num_samples, channels);
}

/**
 * Create the beep detector w/o frame analyzer
 *
 * @param beep to initialize - free with samd_beep_destroy()
 * @param event_handler - callback function when beeps are detected
 * @param user_data to send to callback functions
 */
void samd_beep_init_internal(samd_beep_t **beep)
{
	samd_beep_t *new_beep = (samd_beep_t *)malloc(sizeof(*new_beep));
	samd_beep_set_log_handler(new_beep, null_log_handler, NULL);
	samd_beep_set_event_handler(new_beep, null_event_handler, NULL);

	new_beep->analyzer = NULL;

	*beep = new_beep;
}

/**
 * Create the standalone beep detector
 *
 * @param beep to initialize - free with samd_beep_destroy()
 * @param event_handler - callback function when beeps are detected
 * @param user_data to send to callback functions
 */
void samd_beep_init(samd_beep_t **beep)
{
	samd_beep_t *new_beep;
	samd_beep_init_internal(&new_beep);
	samd_frame_analyzer_init(&new_beep->analyzer);
	samd_frame_analyzer_set_callback(new_beep->analyzer, samd_beep_process_frame, new_beep);
	*beep = new_beep;
}

/**
 * Destroy the detector
 * @param beep
 */
void samd_beep_destroy(samd_beep_t **beep)
{
	if (beep && *beep) {
		samd_frame_analyzer_t *analyzer = (*beep)->analyzer;
		samd_log_printf((*beep), SAMD_LOG_DEBUG, "%d: DESTROY BEEP\n", (*beep)->time_ms);
		if (analyzer) {
			samd_frame_analyzer_destroy(&analyzer);
		}
		free(*beep);
		*beep = NULL;
	}
}
