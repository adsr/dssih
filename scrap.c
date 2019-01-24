/*


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
*/
