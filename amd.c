/* 
 * Copyright (c) 2014 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include "simpleamd.h"
#include "samd_private.h"

static void amd_state_wait_for_voice(samd_t *amd, samd_vad_event_t event);
static void amd_state_detect(samd_t *amd, samd_vad_event_t event);
static void amd_state_human_detected(samd_t *amd, samd_vad_event_t event);
static void amd_state_machine_detected(samd_t *amd, samd_vad_event_t event);
static void amd_state_done(samd_t *amd, samd_vad_event_t event);

/**
 * NO-OP logger
 */
static void null_log_handler(samd_log_level_t level, void *user_data, const char *file, int line, const char *message)
{
	/* ignore */
}

/**
 * NO-OP AMD event handler
 */
static void null_event_handler(samd_event_t event, uint32_t samples, void *user_data)
{
	/* ignore */
}

/**
 * Process VAD events in the wait_for_voice state
 * @param amd
 * @param event VAD event
 */
static void amd_state_wait_for_voice(samd_t *amd, samd_vad_event_t event)
{
	switch (event) {
		case SAMD_VAD_SILENCE_BEGIN:
		case SAMD_VAD_SILENCE:
			if (amd->slices - amd->state_begin >= amd->silence_start_slices) {
				samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: DEAD AIR, transition to DONE\n", amd->slices);
				amd->state_begin = amd->slices;
				amd->state = amd_state_done;
				amd->event_handler(SAMD_DEAD_AIR, amd->samples, amd->user_data);
			}
			break;
		case SAMD_VAD_VOICE_BEGIN:
		case SAMD_VAD_VOICE:
			samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: Start of VOICE, transition to DETECT\n", amd->slices);
			amd->state_begin = amd->slices;
			amd->state = amd_state_detect;
			break;
	}
}

/**
 * Process VAD events in the detect state
 * @param amd
 * @param event VAD event
 */
static void amd_state_detect(samd_t *amd, samd_vad_event_t event)
{
	switch (event) {
		case SAMD_VAD_SILENCE_BEGIN:
		case SAMD_VAD_SILENCE:
			samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: SILENCE, transition to HUMAN DETECTED\n", amd->slices);
			amd->state_begin = amd->slices;
			amd->state = amd_state_human_detected;
			amd->event_handler(SAMD_HUMAN_SILENCE, amd->samples, amd->user_data);
			break;
		case SAMD_VAD_VOICE_BEGIN:
		case SAMD_VAD_VOICE:
			if (amd->slices - amd->state_begin >= amd->machine_slices) {
				samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: Exceeded machine_slices, transition to MACHINE DETECTED\n", amd->slices);
				amd->state_begin = amd->slices;
				amd->state = amd_state_machine_detected;
				amd->event_handler(SAMD_MACHINE_VOICE, amd->samples, amd->user_data);
			}
			break;
	}
}

/**
 * Process VAD events in the human state
 * @param amd
 * @param event VAD event
 */
static void amd_state_human_detected(samd_t *amd, samd_vad_event_t event)
{
	switch (event) {
		case SAMD_VAD_SILENCE_BEGIN:
			amd->event_handler(SAMD_HUMAN_SILENCE, amd->samples, amd->user_data);
			break;
		case SAMD_VAD_SILENCE:
			break;
		case SAMD_VAD_VOICE_BEGIN:
			amd->event_handler(SAMD_HUMAN_VOICE, amd->samples, amd->user_data);
			break;
		case SAMD_VAD_VOICE:
			break;
	}
}

/**
 * Process VAD events in the machine state
 * @param amd
 * @param event VAD event
 */
static void amd_state_machine_detected(samd_t *amd, samd_vad_event_t event)
{
	switch (event) {
		case SAMD_VAD_SILENCE_BEGIN:
			amd->event_handler(SAMD_MACHINE_SILENCE, amd->samples, amd->user_data);
			break;
		case SAMD_VAD_SILENCE:
			break;
		case SAMD_VAD_VOICE_BEGIN:
			amd->event_handler(SAMD_MACHINE_VOICE, amd->samples, amd->user_data);
			break;
		case SAMD_VAD_VOICE:
			break;
	}
}

/**
 * Process VAD events in the done state
 * @param amd
 * @param event VAD event
 */
static void amd_state_done(samd_t *amd, samd_vad_event_t event)
{
	/* done */
}

/**
 * Process VAD events
 * @param event VAD event
 * @param samples sample count this event occurred at
 * @param user_data the AMD
 */
static void vad_event_handler(samd_vad_event_t event, uint32_t samples, void *user_data)
{
	/* forward event to state machine */
	samd_t *amd = (samd_t *)user_data;
	amd->slices++;
	amd->samples = samples;
	amd->state(amd, event);
}

/**
 * Create the AMD
 * @param amd
 * @param event_handler
 * @param user_data
 */
void samd_init(samd_t **amd, samd_event_fn event_handler, void *user_data)
{
	samd_t *new_amd = (samd_t *)malloc(sizeof(*new_amd));

	new_amd->vad = NULL;
	samd_vad_init(&new_amd->vad, vad_event_handler, new_amd);
	samd_vad_set_silence_slices(new_amd->vad, 50);

	if (event_handler) {
		new_amd->event_handler = event_handler;
	} else {
		new_amd->event_handler = null_event_handler;
	}

	new_amd->log_handler = null_log_handler;
	new_amd->user_data = user_data;
	new_amd->state = amd_state_wait_for_voice;
	new_amd->state_begin = 0;
	new_amd->samples = 0;
	new_amd->slices = 0;
	new_amd->silence_start_slices = 200;
	new_amd->machine_slices = 90;

	*amd = new_amd;
}

/**
 * Set the maximum number of silence slices the detector will wait for voice to start
 */
void samd_set_silence_start_slices(samd_t *amd, uint32_t num_slices)
{
	amd->silence_start_slices = num_slices;
}

/**
 * Set the maximum number of voice slices before a machine is detected
 */
void samd_set_machine_slices(samd_t *amd, uint32_t num_slices)
{
	amd->machine_slices = num_slices;
}

/**
 * Set optional logger
 * @param amd
 * @param log_handler
 */
void samd_set_log_handler(samd_t *amd, samd_log_fn log_handler)
{
	amd->log_handler = log_handler;
	samd_vad_set_log_handler(amd->vad, log_handler);
}

/**
 * Define input audio frequency and number of channels
 * @param amd
 * @param sample_rate in Hz
 * @param num_channels >= 1
 */
void samd_set_input_format(samd_t *amd, uint32_t sample_rate, uint32_t num_channels)
{
	samd_vad_set_input_format(amd->vad, sample_rate, num_channels);
}

/**
 * Set VAD energy threshold.  Audio above this level is considered voice.
 * @param amd
 * @param threshold 200 or so seems like a good starting point
 */
void samd_set_threshold(samd_t *amd, uint32_t threshold)
{
	samd_vad_set_threshold(amd->vad, threshold);
}

/**
 * Set the slice size in samples.  The VAD calculates energy per slice and classifies it as either
 * voice or silence.
 * @param amd
 * @param slice_samples
 */
void samd_set_slice_samples(samd_t *amd, uint32_t slice_samples)
{
	samd_vad_set_slice_samples(amd->vad, slice_samples);
}

/**
 * Set the number of consecutive voice slices that trigger a transition to voice
 * @param amd
 * @param num_slices
 */
void samd_set_voice_slices(samd_t *amd, uint32_t num_slices)
{
	samd_vad_set_voice_slices(amd->vad, num_slices);
}

/**
 * Set the number of consecutive silence slices that trigger a transition to silence
 * @param amd
 * @param num_slices
 */
void samd_set_silence_slices(samd_t *amd, uint32_t num_slices)
{
	samd_vad_set_silence_slices(amd->vad, num_slices);
}

/**
 * Process the next buffer of samples
 * @param amd
 * @param samples
 * @param num_samples
 */
void samd_process_buffer(samd_t *amd, int16_t *samples, uint32_t num_samples)
{
	samd_vad_process_buffer(amd->vad, samples, num_samples);
}

/**
 * Destroy the detector
 * @param amd
 */
void samd_destroy(samd_t **amd)
{
	samd_vad_destroy(&(*amd)->vad);
	free(*amd);
	*amd= NULL;
}
