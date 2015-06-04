/* 
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <simpleamd.h>
#include <samd_private.h>

static void amd_state_wait_for_voice(samd_t *amd, samd_vad_event_t event);
static void amd_state_detect(samd_t *amd, samd_vad_event_t event);
static void amd_state_human_detected(samd_t *amd, samd_vad_event_t event);
static void amd_state_machine_detected(samd_t *amd, samd_vad_event_t event);
static void amd_state_done(samd_t *amd, samd_vad_event_t event);

/**
 * NO-OP logger
 */
static void null_log_handler(samd_log_level_t level, void *user_log_data, const char *file, int line, const char *message)
{
	/* ignore */
}

/**
 * NO-OP AMD event handler
 */
static void null_event_handler(samd_event_t event, uint32_t samples, void *user_event_data)
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
			if (amd->time_ms - amd->state_begin_ms >= amd->silence_start_ms) {
				samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: DEAD AIR, transition to DONE\n", amd->time_ms);
				amd->state_begin_ms = amd->time_ms;
				amd->state = amd_state_done;
				amd->event_handler(SAMD_DEAD_AIR, amd->time_ms, amd->user_event_data);
			}
			break;
		case SAMD_VAD_VOICE_BEGIN:
		case SAMD_VAD_VOICE:
			samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: Start of VOICE, transition to DETECT\n", amd->time_ms);
			amd->state_begin_ms = amd->time_ms;
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
			samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: SILENCE, transition to HUMAN DETECTED\n", amd->time_ms);
			amd->state_begin_ms = amd->time_ms;
			amd->state = amd_state_human_detected;
			amd->event_handler(SAMD_HUMAN_SILENCE, amd->time_ms, amd->user_event_data);
			break;
		case SAMD_VAD_VOICE_BEGIN:
		case SAMD_VAD_VOICE:
			if (amd->time_ms - amd->state_begin_ms >= amd->machine_ms) {
				samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: Exceeded machine_ms, transition to MACHINE DETECTED\n", amd->time_ms);
				amd->state_begin_ms = amd->time_ms;
				amd->state = amd_state_machine_detected;
				amd->event_handler(SAMD_MACHINE_VOICE, amd->time_ms, amd->user_event_data);
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
			amd->event_handler(SAMD_HUMAN_SILENCE, amd->time_ms, amd->user_event_data);
			break;
		case SAMD_VAD_SILENCE:
			break;
		case SAMD_VAD_VOICE_BEGIN:
			amd->event_handler(SAMD_HUMAN_VOICE, amd->time_ms, amd->user_event_data);
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
			amd->event_handler(SAMD_MACHINE_SILENCE, amd->time_ms, amd->user_event_data);
			break;
		case SAMD_VAD_SILENCE:
			break;
		case SAMD_VAD_VOICE_BEGIN:
			amd->event_handler(SAMD_MACHINE_VOICE, amd->time_ms, amd->user_event_data);
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
 * Set the maximum duration in ms the detector will wait for voice to start
 */
void samd_set_silence_start_ms(samd_t *amd, uint32_t ms)
{
	amd->silence_start_ms = ms;
}

/**
 * Set the maximum duration in ms before a machine is detected
 */
void samd_set_machine_ms(samd_t *amd, uint32_t ms)
{
	amd->machine_ms = ms;
}

/**
 * Process VAD events
 * @param event VAD event
 * @param time_ms time this event occurred, relative to start of detector
 * @param user_event_data the AMD
 */
static void vad_event_handler(samd_vad_event_t event, uint32_t time_ms, void *user_event_data)
{
	/* forward event to state machine */
	samd_t *amd = (samd_t *)user_event_data;
	amd->time_ms = time_ms;
	amd->state(amd, event);
}

/**
 * Set optional logger
 * @param amd
 * @param log_handler
 */
void samd_set_log_handler(samd_t *amd, samd_log_fn log_handler, void *user_log_data)
{
	amd->user_log_data = user_log_data;
	amd->log_handler = log_handler;
}

/**
 * Set event handler
 * @param amd
 * @param event_handler
 */
void samd_set_event_handler(samd_t *amd, samd_event_fn event_handler, void *user_event_data)
{
	amd->user_event_data = user_event_data;
	amd->event_handler = event_handler;
}

/**
 * Create the AMD
 * @param amd
 * @param vad
 */
void samd_init(samd_t **amd, samd_vad_t *vad)
{
	samd_t *new_amd = (samd_t *)malloc(sizeof(*new_amd));
	samd_set_log_handler(new_amd, null_log_handler, NULL);
	samd_set_event_handler(new_amd, null_event_handler, NULL);

	/* reset AMD state */
	new_amd->state = amd_state_wait_for_voice;
	new_amd->state_begin_ms = 0;
	new_amd->time_ms = 0;

	/* set detection defaults */
	samd_set_silence_start_ms(new_amd, 200);
	samd_set_machine_ms(new_amd, 90);

	/* link to VAD */
	new_amd->vad = vad;
	samd_vad_set_event_handler(vad, vad_event_handler, new_amd);

	*amd = new_amd;
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
	free(*amd);
	*amd = NULL;
}
