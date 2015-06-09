/* 
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#ifndef SAMD_PRIVATE_H
#define SAMD_PRIVATE_H

#include "simpleamd.h"

#define AMD_SAMPLES_PER_MS 8
#define AMD_SAMPLES_PER_FRAME 80
#define AMD_MS_PER_FRAME 10

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

	/** energy threshold - values above this are voice slices */
	uint32_t threshold;

	/** number of consecutive voice frames to trigger transition to voice */
	uint32_t voice_frames;

	/** number of consecutive silence frames to trigger transition to silence */
	uint32_t silence_frames;

	/** user data to send to callbacks */
	void *user_event_data;

	/** user data to send to callbacks */
	void *user_log_data;

	/** current detection state */
	samd_vad_state_fn state;

	/** energy detected in current frame */
	double energy;

	/** last sample processed */
	int16_t last_sample;

	/** zero crossings in current frame */
	uint32_t zero_crossings;

	/** time running */
	uint32_t time_ms;

	/** number of samples processed in current frame */
	uint32_t samples;

	/** total number of samples processed */
	uint32_t total_samples;

	/** number of consecutive voice or silence frames processed prior to transitioning state */
	uint32_t transition_frames;
};

/** Internal AMD state machine function type */
typedef void (* samd_state_fn)(samd_t *amd, samd_vad_event_t event);

/**
 * AMD state
 */
struct samd {
	/** voice activity detector */
	samd_vad_t *vad;

	/** time running */
	uint32_t time_ms;

	/** time spent in voice/silence while in opposite VAD state (e.g consecutive voice while in silence state) */
	uint32_t transition_ms;

	/** maximum frames to wait for voice before giving up */
	uint32_t silence_start_ms;

	/** number of frames that trigger machine detection */
	uint32_t machine_ms;

	/** callback for AMD events */
	samd_event_fn event_handler;

	/** callback for log messages */
	samd_log_fn log_handler;

	/** current detection state */
	samd_state_fn state;

	/** when state was entered */
	uint32_t state_begin_ms;

	/** user data to send to callbacks */
	void *user_event_data;

	/** user data to send to callbacks */
	void *user_log_data;
};

/**
 * Send a log message
 */
#define samd_log_printf(obj, level, format_string, ...)  _samd_log_printf(obj->log_handler, level, obj->user_log_data, __FILE__, __LINE__, format_string, __VA_ARGS__)
void _samd_log_printf(samd_log_fn log_handler, samd_log_level_t level, void *user_data, const char *file, int line, const char *format_string, ...);

#endif
