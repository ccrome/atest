/*
 * Copyright (C) 2015 Arnaud Mouiche <arnaud.mouiche@invoxia.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sched.h>
#include <ev.h>


#include "alsa.h"
#include "log.h"
#include "test.h"
#include "playback.h"
#include "capture.h"
#include "loopback_delay.h"


struct ev_loop *loop = NULL;




/*
 * something to read from stdin
 * read one char at a time, even if this is not efficient.
 */
static void on_stdin( struct ev_loop *loop, struct ev_io *w, int revents ) {
    static char pipecmd[128] = {0};
    char c;
    int r;
    r = read( 0, &c, 1 );
    if (r < 0) {
        /* read failed on stdin. time to exit */
        ev_unloop( loop, EVUNLOOP_ALL);
        return;
    }
    if (c != '\n') {
        /* queue into cmdpipe */
        int l = strlen(pipecmd);
        if (l < sizeof(pipecmd)-2) {
            pipecmd[l] = c;
            pipecmd[l+1] = '\0';
        }
    } else {
        /* execute the command */
        dbg("pipecmd: '%s'", pipecmd);
        if (!strcmp(pipecmd, "q")) {
            warn("quit");
            ev_unloop( loop, EVUNLOOP_ALL);
            return;
        }


        /* reset the pipecmd */
        pipecmd[0] = '\0';
    }

}


/* manage clean shutdown on terminal signal */
static ev_signal evw_intsig, evw_termsig;
static void on_exit_signal(struct ev_loop *loop, ev_signal *w, int revents)
{
    switch (w->signum) {
    case SIGTERM:
        dbg("SIGTERM");
        break;
    case SIGINT:
        dbg("SIGINT");
        break;
    }
    ev_unloop(loop, EVUNLOOP_ALL);
}


static void on_duration_timer( struct ev_loop *loop, struct ev_timer *w, int revents) {
    dbg("end of tests");
    ev_unloop(loop, EVUNLOOP_ALL);
}


static void seq_error_assert(void) {
    dbg("stop on first error");
    ev_unloop(loop, EVUNLOOP_ALL);
}

void usage(void) {
    puts(
        "usage: atest OPTIONS -- TEST [test options] ...\n"
        "OPTIONS:\n"
        "-r, --rate=#             sample rate\n"
        "-c, --channels=#         channels (max 32)\n"
        "-p, --period=FRAMES      period size in number of frames\n"
        "-D, --device=NAME        select PCM by name\n"
        "-C, --config=FILE        use this particular config file\n"
        "-P, --priority=PRIORITY  process priority to set ('fifo,N' 'rr,N' 'other,N')\n"
        "-d, --duration=SECONDS   stop the test after SECONDS\n"
        "-a, --assert             stop on first error detected\n"
        "-I, --invalid-log-size=N how many frames are logged on error (default 1)\n"
        "\n"
        "TEST\n"
        "  play      continuously generate the sequence steam\n"
        "     options:  -x N      simulate a xrun every N ms\n"
        "               -r N,M    stop after N ms of playback,  and restart after M ms\n"
        "\n"
        "  capture   continuously check the received frame sequence\n"
        "     options:  -x N      simulate a xrun every N ms\n"
        "               -r N,M    stop after N ms of playback,  and restart after M ms\n"
        "\n"
        "  loopback_delay   measure the loopback trip time\n"
        "     options:  -a N      assert that the loopback delay equal N frames\n"
        "               -s MODE   start mode: (capture)/play/link\n"
        );
    exit(1);

}


const struct option options[] = {
    { "rate", 1, NULL, 'r' },
    { "channels", 1, NULL, 'c' },
    { "period", 1, NULL, 'p' },
    { "device", 1, NULL, 'D' },
    { "config", 1, NULL, 'C' },
    { "priority", 1, NULL, 'P' },
    { "duration", 1, NULL, 'd' },
    { "assert", 0, NULL, 'a' },
    { "invalid-log-size", 0, NULL, 'I' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char * const argv[]) {

    int result,i,r;
    int opt_index;
    int opt_rate = -1;
    int opt_channels = -1;
    int opt_period = 0;
    int opt_duration = 0;
    int opt_assert = 0;
    int opt_invalid_log_size = 0;
    const char *opt_device = NULL;
    const char *opt_config = NULL;
    const char *opt_priority = NULL;
    struct alsa_config config;

    struct ev_io stdin_watcher;
    struct ev_timer duration_timer;

    loop = ev_default_loop(0);

    while (1) {
        if ((result = getopt_long( argc, argv, "+r:c:p:D:C:P:d:aI:", options, &opt_index )) == EOF) break;
        switch (result) {
        case '?':
            usage();
            break;
        case 'r':
            opt_rate = atoi(optarg);
            break;
        case 'c':
            opt_channels = atoi(optarg);
            break;
        case 'p':
            opt_period = atoi(optarg);
            break;
        case 'd':
            opt_duration = atoi(optarg);
            break;
        case 'D':
            opt_device = optarg;
            break;
        case 'C':
            opt_config = optarg;
            break;
        case 'P':
            opt_priority = optarg;
            break;
        case 'a':
            opt_assert = 1;
            break;
        case 'I':
            opt_invalid_log_size = atoi(optarg);
            break;
        }
    }

    /* generate the config */
    alsa_config_init( &config, opt_config );
    if (opt_rate > 0) config.rate = opt_rate;
    if (opt_channels > 0) config.channels = opt_channels;
    if (opt_period > 0) config.period = opt_period;
    if (opt_device) { strncpy( config.device, opt_device, sizeof(config.device)-1 ); config.device[ sizeof(config.device)-1 ] = '\0'; }
    if (opt_priority) { strncpy( config.priority, opt_priority, sizeof(config.priority)-1 ); config.priority[ sizeof(config.priority)-1 ] = '\0'; }

    /* check if the config is valid */
    if (config.device[0] == '\0') {
        printf("Undefined device.\n");
        exit(1);
    }

    dbg("dev: '%s'", config.device);

#define MAX_TESTS 2
    struct test *tests[MAX_TESTS];
    int tests_count = 0;

    /* build the tests objects */
    argc -= optind;
    argv += optind;

    while (argc) {
        struct test *t = NULL;
        if (!strcmp( argv[0], "play" )) {
            struct playback_create_opts opts = {0};
            optind = 1;
            while (1) {
                if ((result = getopt( argc, argv, "+x:r:" )) == EOF) break;
                switch (result) {
                case '?':
                    printf("invalid option '%s' for test 'play'\n", optarg);
                    usage();
                    break;
                case 'x':
                    opts.xrun = atoi(optarg);
                    break;
                case 'r':
                    if (sscanf(optarg, "%d,%d", &opts.restart_play_time, &opts.restart_pause_time) != 2) {
                        printf("invalid value '%s' for test 'play' option '-r'\n", optarg);
                        usage();
                    }
                    dbg("%d,%d", opts.restart_play_time, opts.restart_pause_time);
                    break;
                }
            }
            argc -= optind-1;
            argv += optind-1;
            t = playback_create( &config, &opts );
            if (!t) {
                err("failed to create a playback test");
                exit(1);
            }
        } else if (!strcmp( argv[0], "capture" )) {
            struct capture_create_opts opts = {0};
            optind = 1;
            while (1) {
                if ((result = getopt( argc, argv, "+x:r:" )) == EOF) break;
                switch (result) {
                case '?':
                    printf("invalid option '%s' for test 'capture'\n", optarg);
                    usage();
                    break;
                case 'x':
                    opts.xrun = atoi(optarg);
                    break;
                case 'r':
                    if (sscanf(optarg, "%d,%d", &opts.restart_play_time, &opts.restart_pause_time) != 2) {
                        printf("invalid value '%s' for test 'capture' option '-r'\n", optarg);
                        usage();
                    }
                    dbg("%d,%d", opts.restart_play_time, opts.restart_pause_time);
                    break;
                }
            }
            argc -= optind-1;
            argv += optind-1;
            t = capture_create( &config, &opts );
            if (!t) {
                err("failed to create a capture test");
                exit(1);
            }
        } else if (!strcmp( argv[0], "loopback_delay" )) {
            struct loopback_delay_create_opts opts = {0};
            optind = 1;
            while (1) {
                if ((result = getopt( argc, argv, "+a:s:x:" )) == EOF) break;
                switch (result) {
                case '?':
                    printf("invalid option '%s' for test 'loopback_delay'\n", optarg);
                    usage();
                    break;
                case 'a':
                    opts.assert_delay = 1;
                    opts.expected_delay = atoi(optarg);
                    break;
                case 's':
                    if (!strcmp(optarg, "capture"))
                        opts.start_sync_mode = LSM_PREPARE_CAPTURE_PLAYBACK;
                    else if (!strcmp(optarg, "play"))
                        opts.start_sync_mode = LSM_PREPARE_PLAYBACK_CAPTURE;
                    else if (!strcmp(optarg, "link"))
                        opts.start_sync_mode = LSM_LINK;
                    else {
                        printf("invalid value '%s' for test 'loopback_delay' option '-s'\n", optarg);
                        usage();
                    }
                    break;
                }
            }
            argc -= optind-1;
            argv += optind-1;
            t = loopback_delay_create( &config, &opts );
            if (!t) {
                err("failed to create a capture test");
                exit(1);
            }
        }

        if (t) {
            if (tests_count >= MAX_TESTS) {
                err("too many tests defined.");
                exit(1);
            }
            tests[tests_count++] = t;
        } else {
            printf("undefined test '%s'.\n", argv[0]);
            usage();
        }
        argc--;
        argv++;
    }
    if (tests_count == 0) {
        printf("no tests specified.\n");
        exit(1);
    }

    /* change the scheduling priority is required */
    if (config.priority[0]) {
        int p;
        if (sscanf(config.priority, "fifo,%d", &p )==1) {
            struct sched_param param;
            param.sched_priority = p;

            dbg("priority: fifo,%d", p);
            if (sched_setscheduler(0, SCHED_FIFO, &param))
               err("sched_setscheduler");
        } else if (sscanf(config.priority, "rr,%d", &p )==1) {
            struct sched_param param;
            param.sched_priority = p;

            dbg("priority: rr,%d", p);
            if (sched_setscheduler(0, SCHED_RR, &param))
                err("sched_setscheduler");
        } else if (sscanf(config.priority, "other,%d", &p )==1) {
            struct sched_param param;
            param.sched_priority = p;

            dbg("priority: other,%d", p);
            if (sched_setscheduler(0, SCHED_OTHER, &param))
                err("sched_setscheduler");
        } else {
            printf("Invalid priority '%s'\n", config.priority);
        }
    }


    /* start the various tests */
    for (i=0; i < tests_count; i++) {
        struct test *t = tests[i];
        r = t->ops->start( t );
        if (r < 0) {
            err("starting test %s failed", t->name );
            exit(1);
        }
    }

    /* setup signal handlers to exist cleanly */
    ev_signal_init(&evw_intsig, on_exit_signal, SIGINT);
    ev_signal_start(loop, &evw_intsig);

    ev_signal_init(&evw_termsig, on_exit_signal, SIGTERM);
    ev_signal_start(loop, &evw_termsig);

    ev_io_init(&stdin_watcher, on_stdin, 0, EV_READ);
    ev_io_start( loop, &stdin_watcher );

    if (opt_assert) {
        seq_error_notify = &seq_error_assert;
    }
    if (opt_invalid_log_size > 0) {
        seq_consecutive_invalid_frames_log = opt_invalid_log_size;
    }

    if (opt_duration > 0) {
        dbg("start a %d seconds duration timer", opt_duration);
        ev_timer_init( &duration_timer, on_duration_timer, opt_duration, 0 );
        ev_timer_start( loop, &duration_timer );
    }

    ev_run( loop, 0 );

    int test_exit_status = 0;
    for (i=0; i < tests_count; i++) {
        struct test *t = tests[i];
        if (t->ops->close( t )) {
            err("%s exit status: failed", t->name);
            test_exit_status = 1;
        }
    }

    printf("total number of sequence errors: %u\n", seq_errors_total);
    printf("global tests exit status: %s\n", test_exit_status ? "FAILED" : "OK");
    /* exit with a good status only if no error was detected */
    return (seq_errors_total || test_exit_status) ? 2 : 0;
}
