/*
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <math.h>
#include "samd_private.h"

static void vad_state_silence(samd_vad_t *vad, int in_voice);
static void vad_state_voice(samd_vad_t *vad, int in_voice);

#define VAD_DEFAULT_ENERGY_THRESHOLD 130.0
#define VAD_DEFAULT_MAX_ENERGY_THRESHOLD 1300.0
#define VAD_DEFAULT_VOICE_MS 60
#define VAD_DEFAULT_VOICE_END_MS 850
#define VAD_DEFAULT_INITIAL_ADJUST_MS 200
#define VAD_DEFAULT_VOICE_ADJUST_MS 0

/**
 * NO-OP log handler
 */
static void null_log_handler(samd_log_level_t level, void *user_data, const char *file, int line, const char *message)
{
	/* ignore */
}

/**
 * NO-OP VAD event handler
 */
static void null_event_handler(samd_vad_event_t event, uint32_t time_ms, uint32_t total_voice_ms, uint32_t transition_ms, void *user_data)
{
	/* ignore */
}

/**
 * Set optional logger
 * @param vad
 * @param log_handler
 */
void samd_vad_set_log_handler(samd_vad_t *vad, samd_log_fn log_handler, void *user_log_data)
{
	vad->user_log_data = user_log_data;
	vad->log_handler = log_handler;
}

/**
 * Set event handler
 * @param vad
 * @param event_handler
 */
void samd_vad_set_event_handler(samd_vad_t *vad, samd_vad_event_fn event_handler, void *user_event_data)
{
	vad->user_event_data = user_event_data;
	vad->event_handler = event_handler;
}

/**
 * Set VAD energy threshold.  Audio frames with energy above this level is considered voice.
 * @param vad
 * @param threshold 200 or so seems like a good starting point
 */
void samd_vad_set_energy_threshold(samd_vad_t *vad, double threshold)
{
	vad->threshold = threshold;
}

/**
 * Set the duration of speech in ms that trigger a transition to voice state
 * @param vad
 * @param ms
 */
void samd_vad_set_voice_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->voice_ms = ms;
}

/**
 * Set the duration of silence in ms to transition to silence state
 * @param vad
 * @param ms
 */
void samd_vad_set_voice_end_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->voice_end_ms = ms;
}

/**
 * Set the sample rate of the audio
 * @param vad
 * @param sample_rate
 */
void samd_vad_set_sample_rate(samd_vad_t *vad, uint32_t sample_rate)
{
	samd_frame_analyzer_set_sample_rate(vad->analyzer, sample_rate);
}

/**
 * Time to adjust energy threshold relative to start.  Set to 0 to disable.
 */
void samd_vad_set_initial_adjust_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->initial_adjust_ms = ms;
}

/**
 * Time to adjust energy threshold relative to start of voice.  Set to 0 to disable.
 */
void samd_vad_set_voice_adjust_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->voice_adjust_ms = ms;
}

/**
 * Maximum energy threshold allowed
 */
void samd_vad_set_max_energy_threshold(samd_vad_t *vad, double max_energy_threshold)
{
	vad->max_threshold = max_energy_threshold;
}

/**
 * Adjust energy threshold based on average energy level.
 */
static void vad_threshold_adjust(samd_vad_t *vad, samd_frame_analyzer_t *analyzer)
{
	uint32_t time_ms = vad->time_ms;
	double average_energy = samd_frame_analyzer_get_average_energy(analyzer);
	double new_threshold = fmin(average_energy * 3.0, vad->max_threshold);
	if (new_threshold > vad->threshold) {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: increasing threshold %f to %f, average energy = %f\n", time_ms, vad->threshold, new_threshold, average_energy);
		vad->threshold = new_threshold;
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: threshold = %f, average energy = %f\n", time_ms, vad->threshold, average_energy);
	}
}

/**
 * Handle the next frame of processed audio
 * @param analzyer the frame analyzer
 * @param user_data this VAD
 */
void samd_vad_process_frame(samd_frame_analyzer_t *analyzer, void *user_data, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	samd_vad_t *vad = (samd_vad_t *)user_data;
	vad->time_ms = time_ms;
	vad->energy = energy;
	vad->zero_crossings = zero_crossings;

	/* auto adjust threshold for noise if configured */
	if (vad->time_ms == vad->initial_adjust_ms || (vad->voice_adjust_ms && vad->initial_voice_time_ms && vad->time_ms == vad->voice_adjust_ms + vad->initial_voice_time_ms)) {
		vad_threshold_adjust(vad, analyzer);
	}

	/* use max energy threshold if sensing of background noise levels has not completed */
	if ((vad->time_ms > vad->initial_adjust_ms && energy > vad->threshold) || energy > vad->max_threshold) {
		vad->total_voice_ms += MS_PER_FRAME;
		vad->state(vad, 1);
	} else {
		vad->state(vad, 0);
	}
}

/**
 * Process the next buffer of samples
 * @param vad
 * @param samples
 * @param num_samples
 * @param channels
 */
void samd_vad_process_buffer(samd_vad_t *vad, int16_t *samples, uint32_t num_samples, uint32_t channels)
{
	samd_frame_analyzer_process_buffer(vad->analyzer, samples, num_samples, channels);
}

/**
 * Create the VAD w/o frame analyzer
 *
 * @param vad to initialize - free with samd_vad_destroy()
 * @param event_handler - callback function when VAD events are detected
 * @param user_data to send to callback functions
 */
void samd_vad_init_internal(samd_vad_t **vad)
{
	samd_vad_t *new_vad = (samd_vad_t *)malloc(sizeof(*new_vad));
	samd_vad_set_log_handler(new_vad, null_log_handler, NULL);
	samd_vad_set_event_handler(new_vad, null_event_handler, NULL);

	new_vad->analyzer = NULL;

	/* reset detection state */
	new_vad->time_ms = 0;
	new_vad->state = vad_state_silence;
	new_vad->transition_ms = 0;
	new_vad->initial_voice_time_ms = 0;
	new_vad->total_voice_ms = 0;

	/* set detection defaults */
	samd_vad_set_energy_threshold(new_vad, VAD_DEFAULT_ENERGY_THRESHOLD);
	samd_vad_set_max_energy_threshold(new_vad, VAD_DEFAULT_MAX_ENERGY_THRESHOLD);
	samd_vad_set_initial_adjust_ms(new_vad, VAD_DEFAULT_INITIAL_ADJUST_MS);
	samd_vad_set_voice_adjust_ms(new_vad, VAD_DEFAULT_VOICE_ADJUST_MS);
	samd_vad_set_voice_ms(new_vad, VAD_DEFAULT_VOICE_MS);
	samd_vad_set_voice_end_ms(new_vad, VAD_DEFAULT_VOICE_END_MS);

	*vad = new_vad;
}

/**
 * Create the standalone VAD
 *
 * @param vad to initialize - free with samd_vad_destroy()
 * @param event_handler - callback function when VAD events are detected
 * @param user_data to send to callback functions
 */
void samd_vad_init(samd_vad_t **vad)
{
	samd_vad_t *new_vad;
	samd_vad_init_internal(&new_vad);

	samd_frame_analyzer_init(&new_vad->analyzer);
	samd_frame_analyzer_set_callback(new_vad->analyzer, samd_vad_process_frame, new_vad);

	*vad = new_vad;
}

/**
 * Process internal events in the silence state
 * @param vad
 * @param in_voice true if a voice frame was measured
 */
static void vad_state_silence(samd_vad_t *vad, int in_voice)
{
	if (in_voice) {
		vad->transition_ms += MS_PER_FRAME;
	} else {
		vad->transition_ms = 0;
	}
	if (vad->transition_ms >= vad->voice_ms) {
		vad->state = vad_state_voice;
		vad->transition_ms = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (silence) VOICE DETECTED, total voice ms = %d\n", vad->time_ms, vad->total_voice_ms);
		if (vad->initial_voice_time_ms == 0) {
			vad->initial_voice_time_ms = vad->time_ms;
		}
		vad->event_handler(SAMD_VAD_VOICE_BEGIN, vad->time_ms, vad->total_voice_ms, 0, vad->user_event_data);
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (silence) energy = %f, voice ms = %d, zero crossings = %d, total voice ms = %d\n", vad->time_ms, vad->energy, vad->transition_ms, vad->zero_crossings, vad->total_voice_ms);
		vad->event_handler(SAMD_VAD_SILENCE, vad->time_ms, vad->total_voice_ms, vad->transition_ms, vad->user_event_data);
	}
}

/**
 * Process internal events in the voice state
 * @param vad
 * @param in_voice true if a voice frame was measured
 */
static void vad_state_voice(samd_vad_t *vad, int in_voice)
{
	if (in_voice) {
		vad->transition_ms = 0;
	} else {
		vad->transition_ms += MS_PER_FRAME;
	}

	if (vad->transition_ms >= vad->voice_end_ms) {
		vad->state = vad_state_silence;
		vad->transition_ms = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (voice) SILENCE DETECTED, total voice ms = %d\n", vad->time_ms, vad->total_voice_ms);
		vad->event_handler(SAMD_VAD_SILENCE_BEGIN, vad->time_ms, vad->total_voice_ms, 0, vad->user_event_data);
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (voice) energy = %f, silence ms = %d, zero crossings = %d, total voice ms = %d\n", vad->time_ms, vad->energy, vad->transition_ms, vad->zero_crossings, vad->total_voice_ms);
		vad->event_handler(SAMD_VAD_VOICE, vad->time_ms, vad->total_voice_ms, vad->transition_ms, vad->user_event_data);
	}
}

/**
 * Destroy the detector
 * @param vad
 */
void samd_vad_destroy(samd_vad_t **vad)
{
	if (vad && *vad) {
		samd_frame_analyzer_t *analyzer = (*vad)->analyzer;
		samd_log_printf((*vad), SAMD_LOG_DEBUG, "%d: DESTROY VAD\n", (*vad)->time_ms);
		if (analyzer) {
			samd_frame_analyzer_destroy(&analyzer);
		}
		free(*vad);
		*vad = NULL;
	}
}

const char *samd_vad_event_to_string(samd_vad_event_t event)
{
	switch (event) {
		case SAMD_VAD_SILENCE_BEGIN: return "VAD SILENCE BEGIN";
		case SAMD_VAD_SILENCE: return "VAD SILENCE";
		case SAMD_VAD_VOICE_BEGIN: return "VAD VOICE BEGIN";
		case SAMD_VAD_VOICE: return "VAD VOICE";
	}
	return "";
}
