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

#define VAD_MS_PER_FRAME 10
#define VAD_SAMPLES_PER_FRAME_DIVISOR 100
#define VAD_INTERNAL_SAMPLE_RATE 8000
#define VAD_DEFAULT_ENERGY_THRESHOLD 130.0
#define VAD_DEFAULT_VOICE_MS 20
#define VAD_DEFAULT_SILENCE_MS 500
#define VAD_DEFAULT_INITIAL_ADJUST_MS 100
#define VAD_DEFAULT_VOICE_ADJUST_MS 50
#define VAD_DEFAULT_THRESHOLD_ADJUST_LIMIT 3

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
 * Set the duration of silence in ms a transition to silence state
 * @param vad
 * @param ms
 */
void samd_vad_set_silence_ms(samd_vad_t *vad, uint32_t ms)
{
	vad->silence_ms = ms;
}

/**
 * Set the sample rate of the audio
 * @param vad
 * @param sample_rate
 */
void samd_vad_set_sample_rate(samd_vad_t *vad, uint32_t sample_rate)
{
	vad->samples_per_frame = sample_rate / VAD_SAMPLES_PER_FRAME_DIVISOR;
	vad->downsample_factor = sample_rate / VAD_INTERNAL_SAMPLE_RATE;
	if (vad->downsample_factor < 1) {
		vad->downsample_factor = 1;
	}

	/* reset any in progress energy calculations */
	vad->samples = 0;
	vad->energy[0] = 0.0;
	vad->energy[1] = 0.0;
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
 * Maximum adjust factor relative to current threshold.
 */
void samd_vad_set_adjust_limit(samd_vad_t *vad, uint32_t limit)
{
	vad->threshold_adjust_limit = limit;
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
	new_vad->energy[0] = 0.0;
	new_vad->energy[1] = 0.0;
	new_vad->samples = 0;
	new_vad->transition_ms = 0;
	new_vad->time_ms = 0;
	new_vad->initial_voice_time_ms = 0;
	new_vad->last_sample = 0;
	new_vad->zero_crossings = 0;
	new_vad->total_energy = 0.0;

	/* set detection defaults */
	samd_vad_set_sample_rate(new_vad, VAD_INTERNAL_SAMPLE_RATE);
	samd_vad_set_energy_threshold(new_vad, VAD_DEFAULT_ENERGY_THRESHOLD);
	samd_vad_set_voice_ms(new_vad, VAD_DEFAULT_VOICE_MS);
	samd_vad_set_silence_ms(new_vad, VAD_DEFAULT_SILENCE_MS);
	samd_vad_set_initial_adjust_ms(new_vad, VAD_DEFAULT_INITIAL_ADJUST_MS);
	samd_vad_set_voice_adjust_ms(new_vad, VAD_DEFAULT_VOICE_ADJUST_MS);
	samd_vad_set_adjust_limit(new_vad, VAD_DEFAULT_THRESHOLD_ADJUST_LIMIT);

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
		vad->transition_ms += VAD_MS_PER_FRAME;
	} else {
		vad->transition_ms = 0;
	}
	if (vad->transition_ms >= vad->voice_ms) {
		vad->state = vad_state_voice;
		vad->transition_ms = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (silence) VOICE DETECTED\n", vad->time_ms);
		if (vad->initial_voice_time_ms == 0) {
			vad->initial_voice_time_ms = vad->time_ms;
		}
		vad->event_handler(SAMD_VAD_VOICE_BEGIN, vad->time_ms, 0, vad->user_event_data);
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (silence) energy = %f, voice ms = %d, zero crossings = %d\n", vad->time_ms, fmax(vad->energy[0], vad->energy[1]), vad->transition_ms, vad->zero_crossings);
		vad->event_handler(SAMD_VAD_SILENCE, vad->time_ms, vad->transition_ms, vad->user_event_data);
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
		vad->transition_ms += VAD_MS_PER_FRAME;
	}

	if (vad->transition_ms >= vad->silence_ms) {
		vad->state = vad_state_silence;
		vad->transition_ms = 0;
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (voice) SILENCE DETECTED\n", vad->time_ms);
		vad->event_handler(SAMD_VAD_SILENCE_BEGIN, vad->time_ms, 0, vad->user_event_data);
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: (voice) energy = %f, silence ms = %d, zero crossings = %d\n", vad->time_ms, fmax(vad->energy[0], vad->energy[1]), vad->transition_ms, vad->zero_crossings);
		vad->event_handler(SAMD_VAD_VOICE, vad->time_ms, vad->transition_ms, vad->user_event_data);
	}
}

/**
 * Adjust energy threshold based on average energy level.
 */
static void vad_threshold_adjust(samd_vad_t *vad)
{
	double average_energy = vad->total_energy / (vad->time_ms / VAD_MS_PER_FRAME);
	double new_threshold = fmin(average_energy, vad->threshold * vad->threshold_adjust_limit);
	if (new_threshold > vad->threshold) {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: increasing threshold %f to %f, average energy = %f\n", vad->time_ms, vad->threshold, new_threshold, average_energy);
		vad->threshold = new_threshold;
	} else {
		samd_log_printf(vad, SAMD_LOG_DEBUG, "%d: threshold = %f, average energy = %f\n", vad->time_ms, vad->threshold, average_energy);
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
	uint32_t i;
	for (i = 0; i < num_samples; i += channels) {
		int32_t mixed_sample = 0;
		uint32_t c;

		vad->samples++;

		/* mix audio for zero crossing data, calculate energy separately per channel */
		for (c = 0; c < channels && c < 2; c++) {
			mixed_sample += samples[i + c];
			if (i % vad->downsample_factor == 0) {
				/* naive downsample */
				vad->energy[c] += abs(samples[i + c]);
			}
		}
		if (mixed_sample > INT16_MAX) {
			mixed_sample = INT16_MAX;
		} else if (mixed_sample < INT16_MIN) {
			mixed_sample = INT16_MIN;
		}

		/* collect zero crossing data - this is a rough measure of frequency and does correlate to 
		   voice activity.  Not currently using this data for voice detection- it is informational for now */
		if (vad->last_sample < 0 && mixed_sample >= 0) {
			vad->zero_crossings++;
		}
		vad->last_sample = mixed_sample;

		if (vad->samples >= vad->samples_per_frame) {
			double energy;

			vad->time_ms += VAD_MS_PER_FRAME;

			/* final energy calculation for frame */
			vad->energy[0] = vad->energy[0] / (vad->samples / vad->downsample_factor);
			vad->energy[1] = vad->energy[1] / (vad->samples / vad->downsample_factor);
			energy = fmax(vad->energy[0], vad->energy[1]);
			vad->total_energy += energy;

			/* auto adjust threshold for noise if configured */
			if (vad->time_ms == vad->initial_adjust_ms ||
				(vad->voice_adjust_ms && vad->initial_voice_time_ms && vad->time_ms == vad->voice_adjust_ms + vad->initial_voice_time_ms)) {
				vad_threshold_adjust(vad);
			}

			/* feed state machine */
			vad->state(vad, energy > vad->threshold);

			/* reset for next frame */
			vad->energy[0] = 0.0;
			vad->energy[1] = 0.0;
			vad->samples = 0;
			vad->zero_crossings = 0;
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
