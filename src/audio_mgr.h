#pragma once
#include <cstdint>

typedef enum {
    AUDIO_UNINIT,   // not yet initialised
    AUDIO_NO_SD,    // SD card not found
    AUDIO_IDLE,     // ready but not playing
    AUDIO_PLAYING,  // actively playing
    AUDIO_PAUSED,   // paused mid-track
} audio_mgr_state_t;

/* Lifecycle */
void              audio_mgr_init();
void              audio_mgr_update();   // call every loop()

/* State & info */
audio_mgr_state_t audio_mgr_get_state();
int               audio_mgr_get_file_count();
const char       *audio_mgr_get_filename(int i);   // basename, no leading /
int               audio_mgr_get_current_index();
const char       *audio_mgr_get_title();
const char       *audio_mgr_get_artist();
uint32_t          audio_mgr_get_position_s();
uint32_t          audio_mgr_get_duration_s();

/* Playback control */
void              audio_mgr_play_index(int i);
void              audio_mgr_toggle_pause();
void              audio_mgr_stop();
void              audio_mgr_next();
void              audio_mgr_prev();

/* Volume (0..21) */
void              audio_mgr_set_volume(int v);
int               audio_mgr_get_volume();
