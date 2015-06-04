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
static void null_event_handler(samd_vad_event_t event, uint32_t samples, void *user_data)
{
	/* ignore */
}

/**
 * Set optional logger
 * @param vad
 * @param log_handler
 */
void samd_vad_set_log_handler(samd_vad_t *vad, samd_log_fn log_handler)
{
	vad->log_handler = log_handler;
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
	vad->voice_frames = floor(ms / AMD_MS_PER_FRAME);
}

/**
 * Set the duration of silence in ms a transition to silence state
 * @param vad
 * @param ms
 */
void samd_vad_set_silence_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->silence_frames = floor(ms / AMD_MS_PER_FRAME);
}

/**
 * Create the VAD
 *
 * @param vad to initialize - free with samd_vad_destroy()
 * @param event_handler - callback function when VAD events are detected
 * @param user_data to send to callback functions
 */
void samd_vad_init(samd_vad_t **vad, samd_vad_event_fn event_handler, void *user_data)
{
	samd_vad_t *new_vad = (samd_vad_t *)malloc(sizeof(*new_vad));

	/* default settings */
	samd_vad_set_log_handler(new_vad, null_log_handler);
	samd_vad_set_energy_threshold(new_vad, 200);
	samd_vad_set_voice_ms(new_vad, 20);
	samd_vad_set_silence_ms(new_vad, 200);

	if (event_handler) {
		new_vad->event_handler = event_handler;
	} else {
		new_vad->event_handler = null_event_handler;
	}
	new_vad->user_data = user_data;

	/* current detection state */
	new_vad->state = vad_state_silence;
	new_vad->energy = 0.0;
	new_vad->samples = 0;
	new_vad->total_samples = 0;
	new_vad->transition_frames = 0;

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
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: energy = %f, voice count = %d\n", vad->total_samples, vad->energy, vad->transition_frames);
		if (++vad->transition_frames >= vad->voice_frames) {
			vad->state = vad_state_voice;
			vad->transition_frames = 0;
			samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: VOICE\n", vad->total_samples);
			vad->event_handler(SAMD_VAD_VOICE_BEGIN, vad->total_samples, vad->user_data);
		}
	} else {
		vad->transition_frames = 0;
		vad->event_handler(SAMD_VAD_SILENCE, vad->total_samples, vad->user_data);
	}
}

/**
 * Process internal events in the voice state
 * @param vad
 * @param in_voice true if a voice frame was measured
 */
static void vad_state_voice(samd_vad_t *vad, int in_voice)
{
	samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: energy = %f, silence count = %d\n", vad->total_samples, vad->energy, vad->transition_frames);
	if (in_voice) {
		vad->transition_frames = 0;
		vad->event_handler(SAMD_VAD_VOICE, vad->total_samples, vad->user_data);
	} else if (++vad->transition_frames >= vad->silence_frames) {
		vad->state = vad_state_silence;
		vad->transition_frames = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: SILENCE\n", vad->total_samples);
		vad->event_handler(SAMD_VAD_SILENCE_BEGIN, vad->total_samples, vad->user_data);
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
		if (vad->samples >= VAD_SAMPLES_PER_FRAME) {
			vad->energy = vad->energy / VAD_SAMPLES_PER_FRAME;
			vad->state(vad, (uint32_t)vad->energy > vad->threshold);
			vad->energy = 0.0;
			vad->samples = 0;
		}
	}
}

/**
 * Destroy the detector
 * @param vad
 */
void samd_vad_destroy(samd_vad_t **vad)
{
	samd_log_printf((*vad), SAMD_LOG_DEBUG, "%d: DESTROY\n", (*vad)->total_samples);
	free(*vad);
	*vad = NULL;
}
