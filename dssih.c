#include "dssih.h"

int dssih_new(ulong sample_rate, size_t frame_count, dssih_t **out_dssih) {
    dssih_t *dssih;
    dssih = calloc(1, sizeof(dssih_t));
    dssih->sample_rate = sample_rate;
    dssih->frame_count = frame_count;
    dssih_plugin_new_func(dssih, "<audio>", dssih_audio_desc_fn, &dssih->audio_plugin);
    dssih_inst_new_by_num(dssih->audio_plugin, 0, &dssih->audio_inst);
    *out_dssih = dssih;
    return 0;
}

int dssih_free(dssih_t *dssih) {
    dssih_plugin_t *plugin, *plugin_tmp;
    dssih_timer_t *timer, *timer_tmp;
    HASH_ITER(hh, dssih->plugin_map, plugin, plugin_tmp) {
        dssih_plugin_free(plugin);
    }
    LL_FOREACH_SAFE2(dssih->timer_list, timer, timer_tmp, next_parent) {
        dssih_timer_free(timer);
    }
    free(dssih);
    return 0;
}

int dssih_plugin_new_module(dssih_t *dssih, const char *path, dssih_plugin_t **out_plugin) {
    void *module;
    DSSI_Descriptor_Function desc_fn;
    dssih_plugin_t *plugin;
    HASH_FIND_STR(dssih->plugin_map, path, plugin);
    if (plugin) {
        // Already exists
        *out_plugin = plugin;
        return 0;
    }
    if (!(module = dlopen(path, RTLD_LAZY))) {
        DSSIH_RETURN_ERR("dssih_plugin_new: dlopen(%s) failed: %s", path, dlerror());
    }
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    // ISO C forbids casting a void ptr to func ptr, but we have do to that
    if (!(desc_fn = (DSSI_Descriptor_Function)dlsym(module, "dssi_descriptor"))) {
        DSSIH_RETURN_ERR("dssih_plugin_new: dlsym(..., 'dssi_descriptor') failed: %s",  dlerror());
    }
    #pragma GCC diagnostic pop
    return dssih_plugin_new(dssih, path, module, desc_fn, out_plugin);
}

int dssih_plugin_new_func(dssih_t *dssih, const char *path, DSSI_Descriptor_Function desc_fn, dssih_plugin_t **out_plugin) {
    return dssih_plugin_new(dssih, path, NULL, desc_fn, out_plugin);
}

int dssih_plugin_new(dssih_t *dssih, const char *path, void *module, DSSI_Descriptor_Function desc_fn, dssih_plugin_t **out_plugin) {
    dssih_plugin_t *plugin;
    plugin = calloc(1, sizeof(dssih_plugin_t));
    plugin->dssih = dssih;
    plugin->module = module;
    plugin->path = strdup(path);
    plugin->dssi_desc_fn = desc_fn;
    HASH_ADD_KEYPTR(hh, dssih->plugin_map, plugin->path, strlen(plugin->path), plugin);
    *out_plugin = plugin;
    return 0;
}

int dssih_plugin_free(dssih_plugin_t *plugin) {
    dssih_inst_t *inst, *inst_tmp;
    LL_FOREACH_SAFE(plugin->inst_list, inst, inst_tmp) {
        dssih_inst_free(inst);
    }
    if (plugin->module) dlclose(plugin->module);
    free(plugin->path);
    free(plugin);
    return 0;
}

int dssih_inst_new_by_num(dssih_plugin_t *plugin, int num, dssih_inst_t **out_inst) {
    const DSSI_Descriptor *desc;
    if (!(desc = plugin->dssi_desc_fn(num))) {
        DSSIH_RETURN_ERR("dssih_inst_new_by_num: id %d not found in %s", num, plugin->path);
    }
    return dssih_inst_new(plugin, desc, out_inst);
}

int dssih_inst_new_by_label(dssih_plugin_t *plugin, const char *label, dssih_inst_t **out_inst) {
    int id;
    const DSSI_Descriptor *desc;
    for (id = 0; (desc = plugin->dssi_desc_fn(id)) != NULL; id++) {
        if (!strcmp(label, desc->LADSPA_Plugin->Label)) {
            return dssih_inst_new(plugin, desc, out_inst);
        }
    }
    DSSIH_RETURN_ERR("dssih_inst_new_by_label: label %s not found in %s", label, plugin->path);
}

int dssih_inst_new(dssih_plugin_t *plugin, const DSSI_Descriptor *desc, dssih_inst_t **out_inst) {
    dssih_inst_t *inst;
    ulong port;
    inst = calloc(1, sizeof(dssih_inst_t));
    inst->plugin = plugin;
    inst->dssi_desc = desc;
    inst->handle = desc->LADSPA_Plugin->instantiate(desc->LADSPA_Plugin, plugin->dssih->sample_rate);
    inst->port_count = desc->LADSPA_Plugin->PortCount;
    inst->port_array = calloc(inst->port_count, sizeof(dssih_port_t));
    for (port = 0; port < inst->port_count; port++) {
        inst->port_array[port].inst = inst;
        inst->port_array[port].port_num = port;
        inst->port_array[port].port_name = desc->LADSPA_Plugin->PortNames[port];
        inst->port_array[port].port_desc = desc->LADSPA_Plugin->PortDescriptors[port];
        desc->LADSPA_Plugin->connect_port(inst->handle, port, &inst->port_array[port].data_null);
    }
    LL_APPEND(plugin->inst_list, inst);
    *out_inst = inst;
    return 0;
}

int dssih_inst_free(dssih_inst_t *inst) {
    LL_DELETE(inst->plugin->inst_list, inst);
    inst->dssi_desc->LADSPA_Plugin->cleanup(inst->handle);
    free(inst->port_array);
    free(inst);
    return 0;
}

int dssih_inst_connect(dssih_inst_t *writer_inst, ulong writer_port_num, dssih_inst_t *reader_inst, ulong reader_port_num, dssih_conn_t **out_conn) {
    dssih_port_t *writer, *reader;
    dssih_conn_t *conn;
    if (writer_port_num >= writer_inst->port_count) {
        DSSIH_RETURN_ERR("dssih_inst_connect: writer_port_num (%lu) >= writer_inst->port_count (%lu)", writer_port_num, writer_inst->port_count);
    }
    if (reader_port_num >= reader_inst->port_count) {
        DSSIH_RETURN_ERR("dssih_inst_connect: reader_port_num (%lu) >= reader_inst->port_count (%lu)", reader_port_num, reader_inst->port_count);
    }
    writer = &writer_inst->port_array[writer_port_num];
    reader = &reader_inst->port_array[reader_port_num];
    LL_FOREACH2(writer_inst->conn_list, conn, next_writer) {
        if (conn->writer == writer && conn->reader == reader) {
            // Already exists
            *out_conn = conn;
            return 0;
        }
    }
    if ((writer->port_desc & LADSPA_PORT_OUTPUT) == 0) {
        DSSIH_RETURN_ERR("%s", "dssih_inst_connect: writer is not an output");
    }
    if ((reader->port_desc & LADSPA_PORT_INPUT) == 0) {
        DSSIH_RETURN_ERR("%s", "dssih_inst_connect: reader is not an input");
    }
    if ((writer->port_desc & LADSPA_PORT_CONTROL) != (reader->port_desc & LADSPA_PORT_CONTROL)) {
        DSSIH_RETURN_ERR("%s", "dssih_inst_connect: type mismatch between writer and reader (control)");
    }
    if ((writer->port_desc & LADSPA_PORT_AUDIO) != (reader->port_desc & LADSPA_PORT_AUDIO)) {
        DSSIH_RETURN_ERR("%s", "dssih_inst_connect: type mismatch between writer and reader (audio)");
    }
    conn = calloc(1, sizeof(dssih_conn_t));
    conn->writer = writer;
    conn->reader = reader;
    if (writer->port_desc & LADSPA_PORT_AUDIO) {
        conn->data_len = writer_inst->plugin->dssih->frame_count;
    } else {
        conn->data_len = 1;
    }
    conn->data = calloc(conn->data_len, sizeof(LADSPA_Data));
    LL_APPEND2(writer_inst->conn_list, conn, next_writer);
    LL_APPEND2(reader_inst->conn_list, conn, next_reader);
    writer_inst->dssi_desc->LADSPA_Plugin->connect_port(writer_inst->handle, writer_port_num, conn->data);
    reader_inst->dssi_desc->LADSPA_Plugin->connect_port(reader_inst->handle, reader_port_num, conn->data);
    *out_conn = conn;
    return 0;
}

int dssih_inst_disconnect(dssih_conn_t *conn) {
    dssih_inst_t *writer_inst, *reader_inst;
    writer_inst = conn->writer->inst;
    reader_inst = conn->reader->inst;
    writer_inst->dssi_desc->LADSPA_Plugin->connect_port(writer_inst->handle, conn->writer->port_num, &conn->writer->data_null);
    reader_inst->dssi_desc->LADSPA_Plugin->connect_port(reader_inst->handle, conn->reader->port_num, &conn->reader->data_null);
    LL_DELETE2(writer_inst->conn_list, conn, next_writer);
    LL_DELETE2(reader_inst->conn_list, conn, next_reader);
    free(conn->data);
    free(conn);
    return 0;
}

int dssih_inst_play(dssih_inst_t *inst, int *notes, int notes_len, int vel, long len_ms) {
    (void)inst;
    (void)notes;
    (void)notes_len;
    (void)vel;
    (void)len_ms;
    DSSIH_RETURN_ERR("%s", "not implemented");
}

int dssih_inst_send_midi(dssih_inst_t *inst, int *bytes, int bytes_len) {
    (void)inst;
    (void)bytes;
    (void)bytes_len;
    DSSIH_RETURN_ERR("%s", "not implemented");
}

int dssih_timer_new(dssih_t *dssih, long delay_ms, long interval_ms, long limit, float pct_change, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer) {
    dssih_timer_t *timer;
    timer = calloc(1, sizeof(dssih_timer_t));
    dssih_ms_to_timespec(delay_ms, &timer->spec.it_value);
    dssih_ms_to_timespec(interval_ms, &timer->spec.it_interval);
    timer->delay_ms = delay_ms;
    timer->interval_ms = interval_ms;
    timer->pct_change = pct_change;
    timer->limit = limit;
    timer->callback = callback;
    timer->udata = udata;
    *out_timer = timer;
    LL_APPEND2(dssih->timer_list, timer, next_parent);
    return 0;
}

int dssih_ms_to_timespec(long ms, struct timespec *spec) {
    long s, ns;
    s = ms / 1000;
    ms -= (s * 1000);
    ns = ms * 1000;
    spec->tv_sec = s;
    spec->tv_nsec = ns;
    return 0;
}

int dssih_timer_new_divide(dssih_timer_t *parent, long divisor, long delay_ms, long limit, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer) {
    return dssih_timer_new_factor(parent, divisor, 1, delay_ms, limit, callback, udata, out_timer);
}

int dssih_timer_new_multiply(dssih_timer_t *parent, long multiplier, long delay_ms, long limit, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer) {
    return dssih_timer_new_factor(parent, multiplier, 0, delay_ms, limit, callback, udata, out_timer);
}

int dssih_timer_new_factor(dssih_timer_t *parent, long factor, int is_divisor, long delay_ms, long limit, dssih_timer_callback_fn *callback, void *udata, dssih_timer_t **out_timer) {
    dssih_timer_t *timer;
    timer = calloc(1, sizeof(dssih_timer_t));
    dssih_ms_to_timespec(delay_ms, &timer->spec.it_value);
    timer->parent = parent;
    timer->delay_ms = delay_ms;
    timer->limit = limit;
    if (is_divisor) {
        timer->divisor = factor;
    } else {
        timer->multiplier = factor;
    }
    timer->callback = callback;
    timer->udata = udata;
    *out_timer = timer;
    LL_APPEND2(parent->child_list, timer, next_child);
    return 0;
}

int dssih_timer_free(dssih_timer_t *timer) {
    (void)timer;
    DSSIH_RETURN_ERR("%s", "not implemented");
}

int dssih_timer_set_interval(dssih_timer_t *timer, long interval_ms) {
    (void)timer;
    (void)interval_ms;
    DSSIH_RETURN_ERR("%s", "not implemented");
}

int dssih_timer_set_count(dssih_timer_t *timer, int count) {
    (void)timer;
    (void)count;
    DSSIH_RETURN_ERR("%s", "not implemented");
}

int dssih_set_midi_handler(dssih_t *dssih, dssih_midi_handler_callback_fn *callback, void *udata) {
    (void)dssih;
    (void)callback;
    (void)udata;
    DSSIH_RETURN_ERR("%s", "not implemented");
}

int dssih_run(dssih_t *dssih) {
    (void)dssih;
    DSSIH_RETURN_ERR("%s", "not implemented");
}
