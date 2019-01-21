#ifndef __DSSIH_H
#define __DSSIH_H

#include <stdio.h>
#include <time.h>
#include <dlfcn.h>

#include <ladspa.h>
#include <dssi.h>
#include <alsa/asoundef.h>
#include <soundio/soundio.h>
#include <uthash.h>
#include <utlist.h>

#define DSSIH_OK  0
#define DSSIH_ERR 1
#define DSSIH_RETURN_ERR(fmt, ...) do {                                 \
    snprintf(_dssih->errstr, sizeof(_dssih->errstr), (fmt), __VA_ARGS__); \
    return DSSIH_ERR;                                                   \
} while (0)

#define OFFSETOF(type, field) ((size_t)&(((type *)0)->field))

#define DSSIH_AUDIO_PORT_OUT_L 0
#define DSSIH_AUDIO_PORT_OUT_R 1
#define DSSIH_AUDIO_PORT_IN_L  2
#define DSSIH_AUDIO_PORT_IN_R  3

typedef unsigned long ulong;
typedef struct _dssih_t dssih_t;
typedef struct _dssih_plugin_t dssih_plugin_t;
typedef struct _dssih_inst_t dssih_inst_t;
typedef struct _dssih_port_t dssih_port_t;
typedef struct _dssih_conn_t dssih_conn_t;
typedef struct _dssih_timer_t dssih_timer_t;
typedef int (*dssih_midi_handler_callback_fn)(snd_seq_event_t *events, ulong events_len, void *udata);
typedef int (*dssih_timer_callback_fn)(dssih_timer_t *timer, void *udata, long count);

struct _dssih_t {
    dssih_plugin_t *plugin_map;
    dssih_timer_t *timer_list;
    dssih_plugin_t *audio_plugin;
    dssih_inst_t *audio_inst;
    ulong sample_rate;
    size_t frame_count;
    char errstr[1024];
};

struct _dssih_plugin_t {
    char *path;
    dssih_t *dssih;
    void *module;
    DSSI_Descriptor_Function dssi_desc_fn;
    dssih_inst_t *inst_list;
    UT_hash_handle hh;
};

struct _dssih_inst_t {
    dssih_plugin_t *plugin;
    const DSSI_Descriptor *dssi_desc;
    LADSPA_Handle handle;                   /* LADSPA_Handle == voidptr */
    ulong port_count;
    dssih_port_t *port_array;
    dssih_conn_t *conn_list;
    dssih_inst_t *next;
};

struct _dssih_port_t {
    dssih_inst_t *inst;
    ulong port_num;
    const char *port_name;
    LADSPA_Data data_null;
    LADSPA_PortDescriptor port_desc;        /* LADSPA_PortDescriptor == int LADSPA_PORT_(INPUT|OUTPUT|CONTROL|AUDIO) */
};

struct _dssih_conn_t {
    dssih_port_t *writer;
    dssih_port_t *reader;
    LADSPA_Data *data;
    size_t data_len;
    dssih_conn_t *next_writer;
    dssih_conn_t *next_reader;
};

struct _dssih_timer_t {
    struct _dssih_timer_t *parent;
    struct _dssih_timer_t *child_list;
    struct itimerspec spec;
    long delay_ms;
    long interval_ms;
    float pct_change;
    long limit;
    long count;
    long divisor;
    long multiplier;
    dssih_timer_callback_fn *callback;
    void *udata;
    dssih_timer_t *next_parent;
    dssih_timer_t *next_child;
};

int dssih_new(ulong sample_rate, size_t frame_count, dssih_t **out_dssih);
int dssih_free(dssih_t *dssih);

int dssih_plugin_new(dssih_t *dssih, const char *path, void *module, DSSI_Descriptor_Function desc_fn, dssih_plugin_t **out_plugin);
int dssih_plugin_new_module(dssih_t *dssih, const char *path, dssih_plugin_t **out_plugin);
int dssih_plugin_new_func(dssih_t *dssih, const char *path, DSSI_Descriptor_Function desc_fn, dssih_plugin_t **out_plugin);
int dssih_plugin_free(dssih_plugin_t *plugin);

int dssih_inst_new_by_num(dssih_plugin_t *plugin, int num, dssih_inst_t **out_inst);
int dssih_inst_new_by_label(dssih_plugin_t *plugin, const char *label, dssih_inst_t **out_inst);
int dssih_inst_new(dssih_plugin_t *plugin, const DSSI_Descriptor *desc, dssih_inst_t **out_inst);
int dssih_inst_free(dssih_inst_t *inst);
int dssih_inst_connect(dssih_inst_t *writer_inst, ulong writer_port_num, dssih_inst_t *reader_inst, ulong reader_port_num, dssih_conn_t **out_conn);
int dssih_inst_disconnect(dssih_conn_t *conn);
int dssih_inst_play(dssih_inst_t *inst, int *notes, int notes_len, int vel, long len_ms);
int dssih_inst_send_midi(dssih_inst_t *inst, int *bytes, int bytes_len);

int dssih_timer_new(dssih_t *dssih, long delay_ms, long interval_ms, long limit, float pct_change, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer);
int dssih_timer_new_divide(dssih_timer_t *parent, long divisor, long delay_ms, long limit, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer);
int dssih_timer_new_multiply(dssih_timer_t *parent, long multiplier, long delay_ms, long limit, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer);
int dssih_timer_new_factor(dssih_timer_t *parent, long factor, int is_divisor, long delay_ms, long limit, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer);
int dssih_timer_free(dssih_timer_t *timer);
int dssih_timer_set_count(dssih_timer_t *timer, int count);
int dssih_timer_set_interval(dssih_timer_t *timer, long interval_ms);

int dssih_set_midi_handler(dssih_t *dssih, dssih_midi_handler_callback_fn *callback, void *udata);
int dssih_run(dssih_t *dssih);
int dssih_ms_to_timespec(long ms, struct timespec *spec);

const DSSI_Descriptor *dssih_audio_desc_fn(ulong index);

dssih_t _dssih_static;
dssih_t *_dssih;

#endif

/*

https://github.com/adsr/dssi/blob/master/dssi/dssi.h
https://github.com/adsr/ladspa-sdk/blob/master/src/ladspa.h
https://github.com/adsr/dssi/blob/master/examples/dssi_analyse_plugin.c

*/
