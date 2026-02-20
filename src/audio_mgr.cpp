#include "audio_mgr.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "i2s_output.h"
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include "pincfg.h"

/* ── Config ─────────────────────────────────────────────────── */
#define MAX_FILES 80

/* ── State ──────────────────────────────────────────────────── */
static audio_mgr_state_t  state      = AUDIO_UNINIT;
static char               filelist[MAX_FILES][128];
static int                file_count = 0;
static int                cur_idx    = -1;
static int                volume     = 10;

/* ── Metadata ─────────────────────────────────────────────── */
static char               s_title[64]  = "";
static char               s_artist[64] = "";

/* ── Time tracking (millis-based, no scrubbing) ────────────── */
static uint32_t           elapsed_ms  = 0;   // ms accumulated before pause
static uint32_t           start_ms    = 0;   // millis() when last unpaused
static uint32_t           pause_fpos  = 0;   // SD byte offset when paused

/* ── Audio objects ─────────────────────────────────────────── */
static I2SOutput         *i2s_out  = nullptr;
static AudioFileSourceSD *file_src = nullptr;
static AudioFileSourceID3 *id3_src = nullptr;
static AudioGeneratorMP3 *mp3_gen  = nullptr;
static AudioGeneratorWAV *wav_gen  = nullptr;
static AudioGenerator    *cur_gen  = nullptr;

/* ── Helpers ─────────────────────────────────────────────────── */
static float vol_gain(int v) { return v / 21.0f; }

static bool is_mp3(const char *n) {
    int l = strlen(n);
    return l >= 5 && strcasecmp(n + l - 4, ".mp3") == 0;
}
static bool is_wav(const char *n) {
    int l = strlen(n);
    return l >= 5 && strcasecmp(n + l - 4, ".wav") == 0;
}
static bool is_audio_ext(const char *n) { return is_mp3(n) || is_wav(n); }

static void MDCallback(void *, const char *type, bool, const char *string) {
    if (!strcmp(type, "TIT2") || !strcmp(type, "Title"))
        strncpy(s_title,  string, sizeof(s_title)  - 1);
    else if (!strcmp(type, "TPE1") || !strcmp(type, "Artist"))
        strncpy(s_artist, string, sizeof(s_artist) - 1);
}

static void cleanup_sources() {
    if (id3_src)  { delete id3_src;  id3_src  = nullptr; }
    if (file_src) { delete file_src; file_src = nullptr; }
}

static void stop_gen() {
    if (cur_gen && cur_gen->isRunning()) cur_gen->stop();
    cur_gen = nullptr;
}

static void start_track(int idx, uint32_t fpos = 0, bool use_id3 = true) {
    file_src = new AudioFileSourceSD(filelist[idx]);
    if (fpos > 0) file_src->seek(fpos, SEEK_SET);

    if (use_id3 && fpos == 0) {
        id3_src = new AudioFileSourceID3(file_src);
        id3_src->RegisterMetadataCB(MDCallback, nullptr);
        cur_gen = is_mp3(filelist[idx]) ? (AudioGenerator*)mp3_gen
                                        : (AudioGenerator*)wav_gen;
        cur_gen->begin(id3_src, i2s_out);
    } else {
        cur_gen = is_mp3(filelist[idx]) ? (AudioGenerator*)mp3_gen
                                        : (AudioGenerator*)wav_gen;
        cur_gen->begin(file_src, i2s_out);
    }
    state    = AUDIO_PLAYING;
    start_ms = millis();
    Serial.printf("[audio] %s (pos=%u)\n", filelist[idx], fpos);
}

/* ── Init ────────────────────────────────────────────────────── */
void audio_mgr_init() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 4000000)) {
        state = AUDIO_NO_SD;
        Serial.println("[audio] SD no encontrada");
        return;
    }
    Serial.println("[audio] SD OK, escaneando...");

    File dir = SD.open("/");
    while (file_count < MAX_FILES) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory() && is_audio_ext(entry.name()))
            snprintf(filelist[file_count++], sizeof(filelist[0]),
                     "/%s", entry.name());
        entry.close();
    }
    dir.close();
    Serial.printf("[audio] %d archivos\n", file_count);

    /* I2S output using old driver/i2s.h API */
    i2s_out = new I2SOutput(I2S_NUM_0, AUDIO_BCLK, AUDIO_WS, AUDIO_DOUT);
    i2s_out->SetGain(vol_gain(volume));

    mp3_gen = new AudioGeneratorMP3();
    wav_gen = new AudioGeneratorWAV();
    state   = AUDIO_IDLE;
}

/* ── Update ──────────────────────────────────────────────────── */
void audio_mgr_update() {
    if (state != AUDIO_PLAYING || !cur_gen) return;

    if (cur_gen->isRunning()) {
        if (!cur_gen->loop()) {
            cur_gen->stop();  /* generator signals done */
        } else {
            return;           /* still playing, nothing more to do */
        }
    }

    /* Track ended ─ auto-advance */
    cleanup_sources();
    elapsed_ms = 0;
    cur_gen    = nullptr;
    int nxt    = cur_idx + 1;
    if (nxt < file_count) {
        s_title[0] = '\0'; s_artist[0] = '\0';
        cur_idx = nxt;
        start_track(cur_idx);
    } else {
        state   = AUDIO_IDLE;
        cur_idx = -1;
        s_title[0] = '\0'; s_artist[0] = '\0';
    }
}

/* ── Queries ─────────────────────────────────────────────────── */
audio_mgr_state_t audio_mgr_get_state()         { return state; }
int               audio_mgr_get_file_count()    { return file_count; }
int               audio_mgr_get_current_index() { return cur_idx; }
const char       *audio_mgr_get_title()         { return s_title; }
const char       *audio_mgr_get_artist()        { return s_artist; }
int               audio_mgr_get_volume()        { return volume; }

const char *audio_mgr_get_filename(int i) {
    if (i < 0 || i >= file_count) return "";
    return filelist[i] + 1;   /* skip leading '/' */
}

uint32_t audio_mgr_get_position_s() {
    if (state == AUDIO_PLAYING)
        return (elapsed_ms + millis() - start_ms) / 1000;
    if (state == AUDIO_PAUSED)
        return elapsed_ms / 1000;
    return 0;
}

uint32_t audio_mgr_get_duration_s() {
    return 0;  /* not cheaply available without pre-parsing */
}

/* ── Control ─────────────────────────────────────────────────── */
void audio_mgr_play_index(int i) {
    if (i < 0 || i >= file_count) return;
    stop_gen();
    cleanup_sources();
    s_title[0] = '\0'; s_artist[0] = '\0';
    elapsed_ms = 0;    pause_fpos  = 0;
    cur_idx    = i;
    start_track(i);
}

void audio_mgr_toggle_pause() {
    if (state == AUDIO_PLAYING) {
        elapsed_ms += millis() - start_ms;
        pause_fpos  = file_src ? (uint32_t)file_src->getPos() : 0;
        stop_gen();
        cleanup_sources();
        state = AUDIO_PAUSED;
    } else if (state == AUDIO_PAUSED) {
        start_track(cur_idx, pause_fpos, false);
    }
}

void audio_mgr_stop() {
    stop_gen();
    cleanup_sources();
    state      = AUDIO_IDLE;
    cur_idx    = -1;
    elapsed_ms = 0;
    s_title[0] = '\0'; s_artist[0] = '\0';
}

void audio_mgr_next() {
    if (file_count == 0) return;
    audio_mgr_play_index(cur_idx < 0 ? 0 : (cur_idx + 1) % file_count);
}

void audio_mgr_prev() {
    if (file_count == 0 || cur_idx < 0) return;
    if (audio_mgr_get_position_s() < 3 && cur_idx > 0)
        audio_mgr_play_index(cur_idx - 1);
    else
        audio_mgr_play_index(cur_idx);  /* restart current */
}

void audio_mgr_set_volume(int v) {
    v = v < 0 ? 0 : (v > 21 ? 21 : v);
    volume = v;
    if (i2s_out) i2s_out->SetGain(vol_gain(v));
}
