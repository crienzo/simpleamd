/* 
 * Copyright (c) 2014 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#ifndef SAMD_PRIVATE_H
#define SAMD_PRIVATE_H

#include "simpleamd.h"

/** internal VAD state machine function type */
typedef void (* samd_vad_state_fn)(samd_vad_t *vad, int in_voice);

/**
 * VAD state
 */
struct samd_vad {
	/** callback for VAD events */
	samd_vad_event_fn event_handler;

	/** callback for log messages */
	samd_log_fn log_handler;

	/** sample rate of input audio */
	uint32_t sample_rate;

	/** number of channels in input audio */
	uint32_t num_channels;

	/** energy threshold - values above this are voice slices */
	uint32_t threshold;

	/** number of samples per slice */
	uint32_t slice_samples;

	/** number of consecutive voice slices to trigger transition to voice */
	uint32_t voice_slices;

	/** number of consecutive silence slices to trigger transition to silence */
	uint32_t silence_slices;

	/** user data to send to callback function */
	void *user_data;

	/** current detection state */
	samd_vad_state_fn state;

	/** energy detected in current slice */
	double energy;

	/** number of samples processed in current slice */
	uint32_t samples;

	/** total number of samples processed */
	uint32_t total_samples;

	/** number of consecutive voice or silence slices processed prior to transitioning state */
	uint32_t transition_slices;
};

/** Internal AMD state machine function type */
typedef void (* samd_state_fn)(samd_t *amd, samd_vad_event_t event);

/**
 * AMD state
 */
struct samd {
	/** voice activity detector */
	samd_vad_t *vad;

	/** number of slices processed */
	uint32_t slices;

	/** number of samples processed */
	uint32_t samples;

	/** maximum slices to wait for voice before giving up */
	uint32_t silence_start_slices;

	/** number of slices that trigger machine detection */
	uint32_t machine_slices;

	/** callback for AMD events */
	samd_event_fn event_handler;

	/** callback for log messages */
	samd_log_fn log_handler;

	/** current detection state */
	samd_state_fn state;

	/** when state was entered */
	uint32_t state_begin;

	/** user data to send to callbacks */
	void *user_data;
};

/**
 * Send a log message
 */
#define samd_log_printf(obj, level, format_string, ...)  _samd_log_printf(obj->log_handler, level, obj->user_data, __FILE__, __LINE__, format_string, __VA_ARGS__)
void _samd_log_printf(samd_log_fn log_handler, samd_log_level_t level, void *user_data, const char *file, int line, const char *format_string, ...);

#endif
