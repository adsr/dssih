#include "dssih.h"

// opt_*

// globals

// func protos

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
    dssih_plugin_t *plugin, *plugin_tmp;
    dssih_inst_t *inst;
    dssih_conn_t *conn;
    int frames_left, frame_count, frame;
    struct SoundIoChannelArea *areas;
    int ch;
    frames_left = frame_count_max;
    while (frames_left > 0) {
        frame_count = frames_left;
        soundio_outstream_begin_write(outstream, &areas, &frame_count);
        HASH_ITER(hh, _dssih.plugin_map, plugin, plugin_tmp) {
            LL_FOREACH(plugin->inst_list, inst) {
                inst->dssi_desc->run_synth(inst->handle, (ulong)frame_count, NULL, 0);
            }
        }
        LL_FOREACH2(_dssih.audio_inst->conn_list, conn, next_reader) {
            ch = (conn->reader->port_num == DSSIH_AUDIO_PORT_OUT_L ? 0 : 1);
            for (frame = 0; frame < frame_count; frame++) {
                *(areas[ch].ptr + areas[ch].step * frame) += conn->data[frame];
            }
        }
        soundio_outstream_end_write(outstream);
        frames_left -= frame_count;
    }
}

TKTK

int main(int argc, char **argv) {
    dssih_plugin_t *plugin;
    plugin = dssih_plugin_open(argv[1]);
    dssih_plugin_describe(plugin);
    dssih_plugin_close(plugin);

TKTK

    pthread_t audio_thread;
    pthread_t timer_thread;

    pthread_create(&audio_thread, NULL, main_audio, NULL);
    pthread_create(&timer_thread, NULL, main_timer, NULL);

    pthread_join(timer_thread, NULL);
    pthread_join(audio_thread, NULL);


    struct SoundIo *soundio;
    struct SoundIoDevice *device;
    struct SoundIoOutStream *outstream;
    int rv;
    int default_out_device_index;
    int err;

    soundio = soundio_create();
    rv = soundio_connect(soundio);
    soundio_flush_events(soundio);

    default_out_device_index = soundio_default_output_device_index(soundio);
    device = soundio_get_output_device(soundio, default_out_device_index);

    fprintf(stderr, "Output device: %s\n", device->name);

    outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->write_callback = write_callback;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
        return 1;
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        return 1;
    }

    for (;;)
        soundio_wait_events(soundio);

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    return 0;
}
