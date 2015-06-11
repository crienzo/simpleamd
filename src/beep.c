/*
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <math.h>
#include "samd_private.h"

#define BEEP_DEFAULT_ENERGY_THRESHOLD 130.0

static void beep_state_wait_for_start(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings);
static void beep_state_collect(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings);
static void beep_state_wait_for_end(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings);
static void beep_state_done(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings);

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

static void beep_reset(samd_beep_t *beep)
{
	beep->start_time = 0;
	beep->beep_zero_crossings = 0;
	beep->other_zero_crossings = 0;
	beep->max_zero_crossings = 0;
	beep->min_zero_crossings = 0;
	beep->max_energy = 0.0;
	beep->min_energy = 0.0;
}

static void process_zero_crossings(samd_beep_t *beep, uint32_t zero_crossings)
{
	/* check if potentially a beep frequency */
	switch (zero_crossings) {
		case 6:
		case 8:
		case 9:
		case 10:
		case 14:
		case 16:
		case 17:
			beep->beep_zero_crossings++;
			beep->max_zero_crossings = zero_crossings > beep->max_zero_crossings ? zero_crossings : beep->max_zero_crossings;
			beep->min_zero_crossings = (zero_crossings < beep->min_zero_crossings || beep->min_zero_crossings == 0) ? zero_crossings : beep->min_zero_crossings;
			break;
		default:
			beep->other_zero_crossings++;
			break;
	}
}

static void beep_state_wait_for_start(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	if (energy > 500.0) {
		samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (start) energy = %f, zero crossings = %d\n", time_ms, energy, zero_crossings);
		beep->max_energy = energy;
		beep->min_energy = energy;
		beep->start_time = time_ms;
		process_zero_crossings(beep, zero_crossings);
		beep->state = beep_state_collect;
	} else {
		samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (wait for start) energy = %f, zero crossings = %d\n", time_ms, energy, zero_crossings);
	}
}

static void beep_state_collect(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	/* beep patterns */
	/* beep     zero crossings    energy    duration
	   0         8, 9             ~2800       480
	   1         10               ~800        780
	   2         9, *10, 11       ~800        300
	   3         5, *6            ~9000       320
	   4         10               ~900        120
	   5         10               ~1500       970
	   6         14               ~1700       210
	   7         10               ~700        160
	   8         8, 9             ~900        360
	   9         8, 9             ~900        350
	   10        8, 9             ~900        370
	   11        10               ~1900       160
	   12        16, 17           ~9000       120
	   13        10               ~800        190
	   14        8, 9             ~1000       360
	   15        14               ~2300       600
	   16        10               ~1400       320
	   17        8, 9             ~2800       580
	   18        10               ~2600       380
	   19        8, 9             ~1000       350
	   20        10               ~1900       170
	*/

	if (energy > beep->min_energy * 0.8 && energy < beep->max_energy * 1.2 && energy > beep->max_energy * 0.5) {
		samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (collect) energy = %f, zero crossings = %d\n", time_ms, energy, zero_crossings);
		beep->max_energy = fmax(energy, beep->max_energy);
		beep->min_energy = fmin(energy, beep->min_energy);
		process_zero_crossings(beep, zero_crossings);
	} else {
		uint32_t duration = time_ms - beep->start_time;
		double pct_good = 0.0;
		uint32_t regularity = beep->max_zero_crossings - beep->min_zero_crossings;
		if (beep->beep_zero_crossings > 0) {
			double good = beep->beep_zero_crossings;
			double bad = beep->other_zero_crossings;
			pct_good = (good / (good + bad)) * 100.0;
		}
		samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (analyze) energy = (%f, %f, %f), zero crossings = (%d, %d, %d), duration = %d, good = %d, bad = %d, %f%%\n",
					time_ms, energy, beep->min_energy, beep->max_energy,
					zero_crossings, beep->min_zero_crossings, beep->max_zero_crossings, duration,
					beep->beep_zero_crossings, beep->other_zero_crossings, pct_good);
		if (duration >= 100 && pct_good > 90.0 && regularity <= 1) {
			samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: POTENTIAL BEEP DETECTED\n", time_ms);
			beep->state = beep_state_wait_for_end;
			beep->start_time = time_ms; /* start counting time from here */
		} else {
			beep_reset(beep);
			beep->state = beep_state_wait_for_start;
		}
	}
}

static void beep_state_wait_for_end(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	/* allow beep to ramp down, then must be silent for threshold */
	if (energy < beep->min_energy * 0.6 || energy < 200) {
		if (time_ms - beep->start_time >= 200) {
			samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (end) BEEP DETECTED\n", time_ms);
			beep->event_handler(time_ms, beep->user_event_data);
			beep_reset(beep);
			beep->state = beep_state_done;
		} else {
			samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (wait for end) energy = %f\n", time_ms, energy);
			beep->min_energy = fmin(energy, beep->min_energy);
		}
	} else {
		/* not a beep */
		samd_log_printf(beep, SAMD_LOG_DEBUG, "%d: (end) NOT A BEEP, energy = %f\n", time_ms, energy);
		beep_reset(beep);
		beep->state = beep_state_wait_for_start;
	}
}

static void beep_state_done(samd_beep_t *beep, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
}

/**
 * Handle the next frame of processed audio
 * @param analzyer the frame analyzer
 * @param user_data this beep detector
 */
void samd_beep_process_frame(samd_frame_analyzer_t *analyzer, void *user_data, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	samd_beep_t *beep = (samd_beep_t *)user_data;
	beep->time_ms = time_ms;
	beep->state(beep, time_ms, energy, zero_crossings);
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
	new_beep->time_ms = 0;
	new_beep->analyzer = NULL;
	new_beep->state = beep_state_wait_for_start;
	beep_reset(new_beep);

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
