/*
 * Copyright (c) 2014-2015 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */

#ifndef SIMPLEAMD_H
#define SIMPLEAMD_H

#include <stdint.h>

/* common */
typedef enum samd_log_level {
	SAMD_LOG_DEBUG,
	SAMD_LOG_INFO,
	SAMD_LOG_WARNING,
	SAMD_LOG_ERROR
} samd_log_level_t;

typedef void (* samd_log_fn)(samd_log_level_t level, void *user_log_data, const char *file, int line, const char *message);

/* VAD */
typedef enum samd_vad_event {
	SAMD_VAD_SILENCE_BEGIN,
	SAMD_VAD_SILENCE,
	SAMD_VAD_VOICE_BEGIN,
	SAMD_VAD_VOICE
} samd_vad_event_t;

typedef struct samd_vad samd_vad_t;

typedef void (* samd_vad_event_fn)(samd_vad_event_t event, uint32_t samples, void *user_event_data);

void samd_vad_init(samd_vad_t **vad);
void samd_vad_set_log_handler(samd_vad_t *vad, samd_log_fn log_handler, void *user_log_data);
void samd_vad_set_event_handler(samd_vad_t *vad, samd_vad_event_fn event_handler, void *user_event_data);
void samd_vad_set_energy_threshold(samd_vad_t *vad, uint32_t energy_threshold);
void samd_vad_set_voice_ms(samd_vad_t *vad, uint32_t ms);
void samd_vad_set_silence_ms(samd_vad_t *vad, uint32_t ms);
void samd_vad_process_buffer(samd_vad_t *vad, int16_t *samples, uint32_t num_samples);
void samd_vad_destroy(samd_vad_t **vad);

/* Answering machine detector */
typedef enum samd_event {
	SAMD_DEAD_AIR,
	SAMD_MACHINE_VOICE,
	SAMD_MACHINE_SILENCE,
	SAMD_HUMAN_VOICE,
	SAMD_HUMAN_SILENCE
} samd_event_t;

typedef struct samd samd_t;
typedef void (* samd_event_fn)(samd_event_t event, uint32_t samples, void *user_event_data);

void samd_init(samd_t **amd, samd_vad_t *vad);
void samd_set_silence_start_ms(samd_t *amd, uint32_t ms);
void samd_set_machine_ms(samd_t *amd, uint32_t ms);
void samd_set_log_handler(samd_t *amd, samd_log_fn log_handler, void *user_log_data);
void samd_set_event_handler(samd_t *amd, samd_event_fn event_handler, void *user_event_data);
void samd_destroy(samd_t **amd);

#endif
