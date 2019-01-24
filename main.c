#include "dssih.h"

// opt_*

// globals

// func protos

static void audio_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
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
        HASH_ITER(hh, _dssih->plugin_map, plugin, plugin_tmp) {
            LL_FOREACH(plugin->inst_list, inst) {
                inst->dssi_desc->run_synth(inst->handle, (ulong)frame_count, NULL, 0);
            }
        }
        LL_FOREACH2(_dssih->audio_inst->conn_list, conn, next_reader) {
            ch = (conn->reader->port_num == DSSIH_AUDIO_PORT_OUT_L ? 0 : 1);
            for (frame = 0; frame < frame_count; frame++) {
                *(areas[ch].ptr + areas[ch].step * frame) += conn->data[frame];
            }
        }
        soundio_outstream_end_write(outstream);
        frames_left -= frame_count;
    }
}

static void timer_callback(dssih_timer_t *timer) {
    int i;
    i = (int)timer->udata;
    printf("timer(%d) count=%lu interval_ms=%lu\n", i, timer->count, timer->interval_ms);
}

static void timer_process(dssih_timer_t *timer, int depth) {
    dssih_timer_t *child;

    if (depth == 0 && timer->parent) {
        if (timer->divisor >= 1 && (timer->count % timer->divisor == 0)) {
            return;
        }
        /*
        } else if (timer->multiplier >= 1 && (timer->parent->count % timer->multiplier == 0)) {
            // Should not get here
            // Multiplied timers at depth==0 should not exist
            timer->count += 1;
            return;
        }
        */
    }

    timer_callback(timer);

    timer->count += 1;

    if (timer->limit > 0 && timer->count >= timer->limit) {
        dssih_timer_disarm(timer);
        timer->is_dead = 1;
        return;
    }

    LL_FOREACH2(timer->child_list, child, next_child) {
        if (child->is_dead) {
            continue;
        } else if (child->divisor >= 1 && (child->count % child->divisor == 0)) {
            dssih_timer_rearm_child(child);
            timer_process(child, depth + 1);
        } else if (child->multiplier >= 1 && ((timer->count - 1) % child->multiplier == 0)) {
            // dssih_timer_rearm_child(child);
            timer_process(child, depth + 1);
        }
    }
}

int main(int argc, char **argv) {
    dssih_timer_t *t1, *t2, *t3, *timer, *timer_tmp;
    int epfd, nfds, i;
    struct epoll_event ev, events[128];
    uint64_t exp;

    memset(&_dssih_static, 0, sizeof(dssih_t));
    _dssih = &_dssih_static;

    /*
    dssih_timer_new_parent(_dssih, 4000, -1, 1.f, (void*)1, t1);
    dssih_timer_new_divide(t1, 3, -1, (void*)2, t2);
    dssih_timer_new_multiply(t1, 4, -1, (void*)3, t3);
    */

    dssih_timer_new_parent(_dssih, 3000, -1, 1.f, (void*)1, &t1);
    dssih_timer_new_divide(t1, 3, -1, (void*)2, &t2);
    dssih_timer_new_divide(t2, 3, -1, (void*)3, &t3);


    epfd = epoll_create(1);
    HASH_ITER(hh, _dssih->timer_map, timer, timer_tmp) {
        dssih_timer_arm(timer);
        ev.events = EPOLLIN;
        ev.data.fd = timer->fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, timer->fd, &ev);
    }

    while (1) {
        nfds = epoll_wait(epfd, events, sizeof(events)/sizeof(struct epoll_event), -1);
        for (i = 0; i < nfds; i++) {
            HASH_FIND_INT(_dssih->timer_map, &events[i].data.fd, timer);
            timer_process(timer, 0);
            read(events[i].data.fd, &exp, sizeof(uint64_t));
        }
    }

    return 0;
}
