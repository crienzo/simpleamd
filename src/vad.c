/*
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <math.h>
#include <simpleamd.h>
#include <samd_private.h>

static void vad_state_silence(samd_vad_t *vad, int in_voice);
static void vad_state_voice(samd_vad_t *vad, int in_voice);


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
static void null_event_handler(samd_vad_event_t event, uint32_t time_ms, uint32_t transition_ms, void *user_data)
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
void samd_vad_set_energy_threshold(samd_vad_t *vad, uint32_t threshold)
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
	vad->voice_frames = (uint32_t)floor(ms / AMD_MS_PER_FRAME);
}

/**
 * Set the duration of silence in ms a transition to silence state
 * @param vad
 * @param ms
 */
void samd_vad_set_silence_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->silence_frames = (uint32_t)floor(ms / AMD_MS_PER_FRAME);
}

/**
 * Create the VAD
 *
 * @param vad to initialize - free with samd_vad_destroy()
 * @param event_handler - callback function when VAD events are detected
 * @param user_data to send to callback functions
 */
void samd_vad_init(samd_vad_t **vad)
{
	samd_vad_t *new_vad = (samd_vad_t *)malloc(sizeof(*new_vad));
	samd_vad_set_log_handler(new_vad, null_log_handler, NULL);
	samd_vad_set_event_handler(new_vad, null_event_handler, NULL);

	/* reset detection state */
	new_vad->state = vad_state_silence;
	new_vad->energy = 0.0;
	new_vad->samples = 0;
	new_vad->total_samples = 0;
	new_vad->transition_frames = 0;
	new_vad->time_ms = 0;
	new_vad->last_sample = 0;
	new_vad->zero_crossings = 0;

	/* set detection defaults */
	samd_vad_set_energy_threshold(new_vad, 130);
	samd_vad_set_voice_ms(new_vad, 20); /* voice if 20 ms uninterrupted */
	samd_vad_set_silence_ms(new_vad, 200); /* silence if 200 ms uninterrupted */

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
		++vad->transition_frames;
	} else {
		vad->transition_frames = 0;
	}
	if (vad->transition_frames >= vad->voice_frames) {
		vad->state = vad_state_voice;
		vad->transition_frames = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (silence) VOICE DETECTED\n", vad->time_ms);
		vad->event_handler(SAMD_VAD_VOICE_BEGIN, vad->time_ms, 0, vad->user_event_data);
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (silence) energy = %f, voice ms = %d, zero crossings = %d\n", vad->time_ms, vad->energy, vad->transition_frames * AMD_MS_PER_FRAME, vad->zero_crossings);
		vad->event_handler(SAMD_VAD_SILENCE, vad->time_ms, vad->transition_frames * AMD_MS_PER_FRAME, vad->user_event_data);
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
		vad->transition_frames = 0;
	} else {
		++vad->transition_frames;
	}

	if (vad->transition_frames >= vad->silence_frames) {
		vad->state = vad_state_silence;
		vad->transition_frames = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (voice) SILENCE DETECTED\n", vad->time_ms);
		vad->event_handler(SAMD_VAD_SILENCE_BEGIN, vad->time_ms, 0, vad->user_event_data);
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (voice) energy = %f, silence ms = %d, zero crossings = %d\n", vad->time_ms, vad->energy, vad->transition_frames * AMD_MS_PER_FRAME, vad->zero_crossings);
		vad->event_handler(SAMD_VAD_VOICE, vad->time_ms, vad->transition_frames * AMD_MS_PER_FRAME, vad->user_event_data);
	}
}

/**
 * Process the next buffer of samples
 * @param vad
 * @param samples
 * @param num_samples
 */
void samd_vad_process_buffer(samd_vad_t *vad, int16_t *samples, uint32_t num_samples)
{
	uint32_t i;
	for (i = 0; i < num_samples; i++) {
		vad->energy += abs(samples[i]);
		vad->samples++;
		vad->total_samples++;
		//if ((vad->last_sample < 0 && samples[i] > 0) || (vad->last_sample > 0 && samples[i] < 0)) {
		if (vad->last_sample < 0 && samples[i] >= 0) {
			vad->zero_crossings++;
		}
		vad->last_sample = samples[i];
		if (vad->samples >= AMD_SAMPLES_PER_FRAME) {
			vad->energy = vad->energy / AMD_SAMPLES_PER_FRAME;
			vad->state(vad, (uint32_t)vad->energy > vad->threshold);
			vad->energy = 0.0;
			vad->samples = 0;
			vad->zero_crossings = 0;
			vad->time_ms += AMD_MS_PER_FRAME;
		}
	}
}

/**
 * Destroy the detector
 * @param vad
 */
void samd_vad_destroy(samd_vad_t **vad)
{
	if (vad && *vad) {
		samd_log_printf((*vad), SAMD_LOG_DEBUG, "%d: DESTROY VAD\n", (*vad)->time_ms);
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
