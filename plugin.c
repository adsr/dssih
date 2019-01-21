#include "dssih.h"

static DSSI_Descriptor dssi_desc;
static LADSPA_Descriptor ladspa_desc;
static LADSPA_PortDescriptor port_desc[4];
static const char *port_name[4];
static int inst;

static LADSPA_Handle dssih_audio_instantiate(const LADSPA_Descriptor *desc, ulong sample_rate);
static void dssih_audio_cleanup(LADSPA_Handle i);
static void dssih_audio_activate(LADSPA_Handle i);
static void dssih_audio_deactivate(LADSPA_Handle i);
static void dssih_audio_run_synth(LADSPA_Handle i, ulong c1, snd_seq_event_t *e, ulong c2);
static void dssih_audio_run(LADSPA_Handle i, ulong c);
static void dssih_audio_connect_port(LADSPA_Handle i, ulong port, LADSPA_Data *data);

const DSSI_Descriptor *dssih_audio_desc_fn(ulong index) {
    if (index != 0) return NULL;
    memset(&dssi_desc, 0, sizeof(DSSI_Descriptor));
    memset(&ladspa_desc, 0, sizeof(LADSPA_Descriptor));
    memset(port_desc, 0, 4 * sizeof(LADSPA_PortDescriptor));
    memset(port_name, 0, 4 * sizeof(char *));
    port_desc[DSSIH_AUDIO_PORT_OUT_L] = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    port_desc[DSSIH_AUDIO_PORT_OUT_R] = LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO;
    port_desc[DSSIH_AUDIO_PORT_IN_L]  = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    port_desc[DSSIH_AUDIO_PORT_IN_R]  = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    port_name[DSSIH_AUDIO_PORT_OUT_L] = "audio_out_left";
    port_name[DSSIH_AUDIO_PORT_OUT_R] = "audio_out_right";
    port_name[DSSIH_AUDIO_PORT_IN_L]  = "audio_in_left";
    port_name[DSSIH_AUDIO_PORT_IN_R]  = "audio_in_right";
    ladspa_desc.Label = "dssih_audio";
    ladspa_desc.Name = "dssih audio";
    ladspa_desc.PortDescriptors = port_desc;
    ladspa_desc.PortNames = port_name;
    ladspa_desc.instantiate = dssih_audio_instantiate;
    ladspa_desc.cleanup = dssih_audio_cleanup;
    ladspa_desc.activate = dssih_audio_activate;
    ladspa_desc.deactivate = dssih_audio_deactivate;
    ladspa_desc.run = dssih_audio_run;
    ladspa_desc.connect_port = dssih_audio_connect_port;
    dssi_desc.DSSI_API_Version = 1;
    dssi_desc.LADSPA_Plugin = &ladspa_desc;
    dssi_desc.run_synth = dssih_audio_run_synth;
    return &dssi_desc;
}

static LADSPA_Handle dssih_audio_instantiate(const LADSPA_Descriptor *desc, ulong sample_rate) {
    (void)desc;
    (void)sample_rate;
    return (LADSPA_Handle)(&inst);
}

static void dssih_audio_cleanup(LADSPA_Handle i) {
    (void)i;
}

static void dssih_audio_activate(LADSPA_Handle i) {
    (void)i;
}

static void dssih_audio_deactivate(LADSPA_Handle i) {
    (void)i;
}

static void dssih_audio_run_synth(LADSPA_Handle i, ulong c1, snd_seq_event_t *e, ulong c2) {
    (void)i;
    (void)c1;
    (void)e;
    (void)c2;
}

static void dssih_audio_run(LADSPA_Handle i, ulong c) {
    (void)i;
    (void)c;
}

static void dssih_audio_connect_port(LADSPA_Handle i, ulong port, LADSPA_Data *data) {
    (void)i;
    (void)port;
    (void)data;
}
