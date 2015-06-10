/*
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <math.h>
#include "samd_private.h"

#define SAMPLES_PER_FRAME_DIVISOR 100
#define INTERNAL_SAMPLE_RATE 8000

/**
 * Set the sample rate of the audio
 * @param frame_analyzer
 * @param sample_rate
 */
void samd_frame_analyzer_set_sample_rate(samd_frame_analyzer_t *analyzer, uint32_t sample_rate)
{
	analyzer->samples_per_frame = sample_rate / SAMPLES_PER_FRAME_DIVISOR;
	analyzer->downsample_factor = sample_rate / INTERNAL_SAMPLE_RATE;
	if (analyzer->downsample_factor < 1) {
		analyzer->downsample_factor = 1;
	}

	/* reset in progress frame calculations */
	analyzer->samples = 0;
	analyzer->energy[0] = 0.0;
	analyzer->energy[1] = 0.0;
	analyzer->zero_crossings = 0;
}

/**
 * Initialize the frame_analyzer
 *
 * @param analyzer to initialize - free with samd_frame_analyzer_destroy()
 */
void samd_frame_analyzer_init(samd_frame_analyzer_t **analyzer)
{
	samd_frame_analyzer_t *new_analyzer = (samd_frame_analyzer_t *)malloc(sizeof(*new_analyzer));

	new_analyzer->energy[0] = 0.0;
	new_analyzer->energy[1] = 0.0;
	new_analyzer->samples = 0;
	new_analyzer->time_ms = 0;
	new_analyzer->last_sample = 0;
	new_analyzer->zero_crossings = 0;
	new_analyzer->total_energy = 0.0;
	new_analyzer->callback = NULL;

	samd_frame_analyzer_set_sample_rate(new_analyzer, INTERNAL_SAMPLE_RATE);

	*analyzer = new_analyzer;
}

/**
 * Set event handler
 * @param vad
 * @param event_handler
 */
void samd_frame_analyzer_set_callback(samd_frame_analyzer_t *analyzer, samd_frame_analyzer_cb_fn cb, void *user_cb_data)
{
	analyzer->user_cb_data = user_cb_data;
	analyzer->callback = cb;
}

/**
 * Process the next buffer of samples
 * @param frame_analyzer
 * @param samples
 * @param num_samples
 * @param channels
 */
void samd_frame_analyzer_process_buffer(samd_frame_analyzer_t *analyzer, int16_t *samples, uint32_t num_samples, uint32_t channels)
{
	uint32_t i;
	for (i = 0; i < num_samples; i += channels) {
		int32_t mixed_sample = 0;
		uint32_t c;

		analyzer->samples++;

		/* mix audio for zero crossing data, calculate energy separately per channel */
		for (c = 0; c < channels && c < 2; c++) {
			mixed_sample += samples[i + c];
			if (i % analyzer->downsample_factor == 0) {
				/* naive downsample */
				analyzer->energy[c] += abs(samples[i + c]);
			}
		}
		if (mixed_sample > INT16_MAX) {
			mixed_sample = INT16_MAX;
		} else if (mixed_sample < INT16_MIN) {
			mixed_sample = INT16_MIN;
		}

		/* collect zero crossing data - this is a rough measure of frequency and does correlate to voice / unvoiced speech. */
		if (analyzer->last_sample < 0 && mixed_sample >= 0) {
			analyzer->zero_crossings++;
		}
		analyzer->last_sample = mixed_sample;

		if (analyzer->samples >= analyzer->samples_per_frame) {
			double energy;

			analyzer->time_ms += MS_PER_FRAME;

			/* final energy calculation for frame */
			analyzer->energy[0] = analyzer->energy[0] / (analyzer->samples / analyzer->downsample_factor);
			analyzer->energy[1] = analyzer->energy[1] / (analyzer->samples / analyzer->downsample_factor);
			energy = fmax(analyzer->energy[0], analyzer->energy[1]);
			analyzer->total_energy += energy;

			/* send frame information */
			analyzer->callback(analyzer, analyzer->user_cb_data, analyzer->time_ms, energy, analyzer->zero_crossings);

			/* reset for next frame */
			analyzer->energy[0] = 0.0;
			analyzer->energy[1] = 0.0;
			analyzer->samples = 0;
			analyzer->zero_crossings = 0;
		}
	}
}

double samd_frame_analyzer_get_average_energy(samd_frame_analyzer_t *analyzer)
{
	return analyzer->total_energy / (analyzer->time_ms / MS_PER_FRAME);
}

/**
 * Destroy the detector
 * @param analyzer
 */
void samd_frame_analyzer_destroy(samd_frame_analyzer_t **analyzer)
{
	if (analyzer && *analyzer) {
		free(*analyzer);
		*analyzer = NULL;
	}
}

