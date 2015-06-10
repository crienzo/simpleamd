/* 
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#include <stdlib.h>
#include <simpleamd.h>
#include <samd_private.h>

static void amd_state_wait_for_voice(samd_t *amd, samd_vad_event_t event, int beep);
static void amd_state_detect(samd_t *amd, samd_vad_event_t event, int beep);
static void amd_state_human_detected(samd_t *amd, samd_vad_event_t event, int beep);
static void amd_state_machine_detected(samd_t *amd, samd_vad_event_t event, int beep);
static void amd_state_done(samd_t *amd, samd_vad_event_t event, int beep);

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
 * @param beep
 */
static void amd_state_wait_for_voice(samd_t *amd, samd_vad_event_t event, int beep)
{
	if (beep) {
		samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: BEEP, transition to MACHINE DETECTED\n", amd->time_ms);
		amd->state_begin_ms = amd->time_ms;
		amd->state = amd_state_machine_detected;
		amd->event_handler(SAMD_MACHINE_BEEP, amd->time_ms, amd->user_event_data);
		return;
	}

	switch (event) {
		case SAMD_VAD_SILENCE_BEGIN:
		case SAMD_VAD_SILENCE:
			if (amd->time_ms - amd->state_begin_ms >= amd->silence_start_ms) {
				samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: NO VOICE, transition to DONE\n", amd->time_ms);
				amd->state_begin_ms = amd->time_ms;
				amd->state = amd_state_done;
				amd->event_handler(SAMD_NO_VOICE, amd->time_ms, amd->user_event_data);
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
 * @param beep
 */
static void amd_state_detect(samd_t *amd, samd_vad_event_t event, int beep)
{
	if (beep) {
		samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: BEEP, transition to MACHINE DETECTED\n", amd->time_ms);
		amd->state_begin_ms = amd->time_ms;
		amd->state = amd_state_machine_detected;
		amd->event_handler(SAMD_MACHINE_BEEP, amd->time_ms, amd->user_event_data);
		return;
	}

	switch (event) {
		case SAMD_VAD_NONE:
			break;
		case SAMD_VAD_SILENCE_BEGIN:
		case SAMD_VAD_SILENCE:
			samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: SILENCE, transition to HUMAN DETECTED\n", amd->time_ms);
			amd->state_begin_ms = amd->time_ms;
			amd->state = amd_state_human_detected;
			amd->event_handler(SAMD_HUMAN_SILENCE, amd->time_ms, amd->user_event_data);
			break;
		case SAMD_VAD_VOICE_BEGIN:
		case SAMD_VAD_VOICE:
			/* calculate time in voice minus any current silence (transition ms) we are hearing */
			if (amd->time_ms - amd->state_begin_ms - amd->transition_ms >= amd->machine_ms) {
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
 * @param beep
 */
static void amd_state_human_detected(samd_t *amd, samd_vad_event_t event, int beep)
{
	if (beep) {
		samd_log_printf(amd, SAMD_LOG_DEBUG, "%d: BEEP, transition to MACHINE DETECTED\n", amd->time_ms);
		amd->state_begin_ms = amd->time_ms;
		amd->state = amd_state_machine_detected;
		amd->event_handler(SAMD_MACHINE_BEEP, amd->time_ms, amd->user_event_data);
		return;
	}

	switch (event) {
		case SAMD_VAD_NONE:
			break;
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
 * @param beep
 */
static void amd_state_machine_detected(samd_t *amd, samd_vad_event_t event, int beep)
{
	switch (event) {
		case SAMD_VAD_NONE:
			break;
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
 * @param beep
 */
static void amd_state_done(samd_t *amd, samd_vad_event_t event, int beep)
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
 * @param transition_ms duration spent in voice/silence while in opposite state
 * @param user_event_data the AMD
 */
static void vad_event_handler(samd_vad_event_t event, uint32_t time_ms, uint32_t transition_ms, void *user_event_data)
{
	/* forward event to state machine */
	samd_t *amd = (samd_t *)user_event_data;
	amd->time_ms = time_ms;
	amd->transition_ms = transition_ms;
	amd->state(amd, event, 0);
}

/**
 * Process beep events
 * @param time_ms time this event occurred, relative to start of detector
 * @param user_event_data the AMD
 */
static void beep_event_handler(uint32_t time_ms, void *user_event_data)
{
	/* forward event to state machine */
	samd_t *amd = (samd_t *)user_event_data;
	amd->time_ms = time_ms;
	amd->state(amd, SAMD_VAD_NONE, 1);
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
	samd_vad_set_log_handler(amd->vad, log_handler, user_log_data);
	samd_beep_set_log_handler(amd->beep, log_handler, user_log_data);
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
 * Handle the next frame of processed audio
 * @param analzyer the frame analyzer
 * @param user_data this detector
 */
static void process_frame(samd_frame_analyzer_t *analyzer, void *user_data, uint32_t time_ms, double energy, uint32_t zero_crossings)
{
	samd_t *amd = (samd_t *)user_data;
	samd_beep_process_frame(analyzer, amd->beep, time_ms, energy, zero_crossings);
	samd_vad_process_frame(analyzer, amd->vad, time_ms, energy, zero_crossings);
}

/**
 * Process the next buffer of samples
 * @param amd
 * @param samples
 * @param num_samples
 * @param channels
 */
void samd_process_buffer(samd_t *amd, int16_t *samples, uint32_t num_samples, uint32_t channels)
{
	samd_frame_analyzer_process_buffer(amd->analyzer, samples, num_samples, channels);
}

/**
 * Create the AMD
 * @param amd
 */
void samd_init(samd_t **amd)
{
	samd_t *new_amd = (samd_t *)malloc(sizeof(*new_amd));

	/* Link to common frame analyzer for VAD and beep */
	samd_frame_analyzer_init(&new_amd->analyzer);
	samd_frame_analyzer_set_callback(new_amd->analyzer, process_frame, new_amd);

	/* link to VAD and beep detectors */
	samd_vad_init_internal(&new_amd->vad);
	samd_vad_set_event_handler(new_amd->vad, vad_event_handler, new_amd);
	samd_beep_init_internal(&new_amd->beep);
	samd_beep_set_event_handler(new_amd->beep, beep_event_handler, new_amd);

	samd_set_log_handler(new_amd, null_log_handler, NULL);
	samd_set_event_handler(new_amd, null_event_handler, NULL);

	/* reset AMD state */
	new_amd->state = amd_state_wait_for_voice;
	new_amd->state_begin_ms = 0;
	new_amd->time_ms = 0;

	/* set detection defaults */
	samd_set_silence_start_ms(new_amd, 2000); /* wait 2 seconds for start of speech */
	samd_set_machine_ms(new_amd, 1100); /* machine if at least 1100 ms of voice */

	*amd = new_amd;
}

/**
 * Destroy the detector
 * @param amd
 */
void samd_destroy(samd_t **amd)
{
	if (amd && *amd) {
		samd_t *a = *amd;
		samd_log_printf(a, SAMD_LOG_DEBUG, "%d: DESTROY AMD\n", a->time_ms);
		if (a->analyzer) {
			samd_frame_analyzer_destroy(&a->analyzer);
		}
		if (a->beep) {
			samd_beep_destroy(&a->beep);
		}
		if (a->vad) {
			samd_vad_destroy(&a->vad);
		}
		free(a);
		*amd = NULL;
	}
}

const char *samd_event_to_string(samd_event_t event)
{
	switch (event) {
		case SAMD_NO_VOICE: return "AMD NO VOICE";
		case SAMD_MACHINE_VOICE: return "AMD MACHINE VOICE";
		case SAMD_MACHINE_SILENCE: return "AMD MACHINE SILENCE";
		case SAMD_MACHINE_BEEP: return "AMD MACHINE BEEP";
		case SAMD_HUMAN_VOICE: return "AMD HUMAN VOICE";
		case SAMD_HUMAN_SILENCE: return "AMD HUMAN SILENCE";
	}
	return "";
}

void samd_set_sample_rate(samd_t *amd, uint32_t sample_rate)
{
	samd_frame_analyzer_set_sample_rate(amd->analyzer, sample_rate);
}

samd_vad_t *samd_get_vad(samd_t *amd)
{
	return amd->vad;
}

samd_beep_t *samd_get_beep(samd_t *amd)
{
	return amd->beep;
}
