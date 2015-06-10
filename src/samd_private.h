/* 
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#ifndef SAMD_PRIVATE_H
#define SAMD_PRIVATE_H

#include "simpleamd.h"

#define MS_PER_FRAME 10

typedef struct samd_frame_analyzer samd_frame_analyzer_t;

/** internal VAD state machine function type */
typedef void (* samd_vad_state_fn)(samd_vad_t *vad, int in_voice);

/** frame analyzer callback function */
typedef void (* samd_frame_analyzer_cb_fn)(samd_frame_analyzer_t *analyzer, void *user_data, uint32_t time_ms, double energy, uint32_t zero_crossings);

/**
 * Frame analysis stats
 */
struct samd_frame_analyzer {

	/** callback for frame data */
	samd_frame_analyzer_cb_fn callback;

	/** user data to send to callbacks */
	void *user_cb_data;

	/** energy detected in current frame channels (mono or stereo only) */
	double energy[2];

	/** total energy observed */
	double total_energy;

	/** normalizes energy calculation over different sample rates */
	uint32_t downsample_factor;

	/** last sample processed */
	int16_t last_sample;

	/** zero crossings in current frame */
	uint32_t zero_crossings;

	/** time running */
	uint32_t time_ms;

	/** number of frames processed */
	uint32_t frames;

	/** number of samples processed in current frame */
	uint32_t samples;

	uint32_t samples_per_frame;
};

/**
 * VAD state
 */
struct samd_vad {
	/** current frame state */
	samd_frame_analyzer_t *analyzer;

	/** callback for VAD events */
	samd_vad_event_fn event_handler;

	/** callback for log messages */
	samd_log_fn log_handler;

	/** time running */
	uint32_t time_ms;

	/** energy detected in current frame */
	double energy;

	/** zero crossings in current frame */
	uint32_t zero_crossings;

	/** energy threshold - values above this are voice slices */
	double threshold;

	/** duration of voice to trigger transition to voice */
	uint32_t voice_ms;

	/** duration of silence to trigger transition to silence */
	uint32_t silence_ms;

	/** user data to send to callbacks */
	void *user_event_data;

	/** user data to send to callbacks */
	void *user_log_data;

	/** current detection state */
	samd_vad_state_fn state;

	/** maximum factor to adjust threshold relative to current threshold. */
	uint32_t threshold_adjust_limit;

	/** time relative to start to adjust energy threshold.  0 to disable. */
	uint32_t initial_adjust_ms;

	/** time relative to start of voice to adjust energy threshold.  0 to disable. */
	uint32_t voice_adjust_ms;

	/** duration of voice or silence processed prior to transitioning state */
	uint32_t transition_ms;

	/** Time when first speech heard */
	uint32_t initial_voice_time_ms;
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

void samd_frame_analyzer_init(samd_frame_analyzer_t **analyzer);
void samd_frame_analyzer_set_callback(samd_frame_analyzer_t *analyzer, samd_frame_analyzer_cb_fn cb, void *user_cb_data);
void samd_frame_analyzer_set_sample_rate(samd_frame_analyzer_t *analyzer, uint32_t sample_rate);
void samd_frame_analyzer_process_buffer(samd_frame_analyzer_t *analyzer, int16_t *samples, uint32_t num_samples, uint32_t channels);
double samd_frame_analyzer_get_average_energy(samd_frame_analyzer_t *analyzer);
void samd_frame_analyzer_destroy(samd_frame_analyzer_t **analyzer);

#endif
