// SPDX-License-Identifier: CC-BY-4.0
// Copyright (c) 2026 sergeych and Ticktack contributors
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define RATE 44100
#define STRIKE_SPACING 69898 /* round(1.585 * RATE), matching the approved demo */
static volatile sig_atomic_t stopping;
static char config_dir[PATH_MAX];
struct Sound { unsigned char *map; size_t size, frames; const unsigned char *pcm; };
struct Chime {
    struct Sound quarters[3], prefix, strike;
    size_t position, length;
    int quarter, hits, active;
};
static void fail(const char *what) { perror(what); exit(1); }
static void on_signal(int sig) { (void)sig; stopping = 1; }
static uint16_t u16(const unsigned char *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t u32(const unsigned char *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }
static int sample(const struct Sound *s, size_t frame) { return (int16_t)u16(s->pcm + frame * 2); }
static void notify_ready(void) {
    const char *name = getenv("NOTIFY_SOCKET");
    if (!name || !*name) return;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    size_t len = strlen(name);
    if (len >= sizeof address.sun_path) { errno = ENAMETOOLONG; fail("notify socket"); }
    memcpy(address.sun_path, name, len + 1);
    socklen_t size = offsetof(struct sockaddr_un, sun_path) + len + 1;
    if (*name == '@') { address.sun_path[0] = '\0'; size--; }
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0 || sendto(fd, "READY=1", 7, MSG_NOSIGNAL, (struct sockaddr *)&address, size) != 7)
        fail("notify ready");
    close(fd);
}

static void path_join(char *out, const char *dir, const char *name) {
    int n = snprintf(out, PATH_MAX, "%s/%s", dir, name);
    if (n < 0 || n >= PATH_MAX) { errno = ENAMETOOLONG; fail("path"); }
}
static void config_paths(void) {
    const char *base = getenv("XDG_CONFIG_HOME");
    char parent[PATH_MAX];
    if (base && *base == '/') path_join(config_dir, base, "ticktack");
    else {
        base = getenv("HOME");
        if (!base || *base != '/') { fputs("HOME must be absolute\n", stderr); exit(1); }
        path_join(parent, base, ".config");
        if (mkdir(parent, 0700) && errno != EEXIST) fail("mkdir config");
        path_join(config_dir, parent, "ticktack");
    }
    if (mkdir(config_dir, 0700) && errno != EEXIST) fail("mkdir ticktack");
}
static int read_setting(const char *name, int fallback, int maximum) {
    char path[PATH_MAX]; path_join(path, config_dir, name);
    FILE *f = fopen(path, "r");
    if (!f) { if (errno == ENOENT) return fallback; fail("read setting"); }
    int value = -1; char extra;
    int fields = fscanf(f, "%d %c", &value, &extra); fclose(f);
    if (fields != 1 || value < 0 || value > maximum) {
        fprintf(stderr, "%s must contain an integer from 0 to %d\n", name, maximum); exit(1);
    }
    return value;
}
static void save_text(const char *name, const char *text) {
    char path[PATH_MAX], temp[PATH_MAX], suffix[64];
    path_join(path, config_dir, name);
    snprintf(suffix, sizeof suffix, ".%s-%ld", name, (long)getpid());
    path_join(temp, config_dir, suffix);
    int fd = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) fail("create setting");
    size_t len = strlen(text);
    if (write(fd, text, len) != (ssize_t)len || close(fd)) { unlink(temp); fail("write setting"); }
    if (rename(temp, path)) { unlink(temp); fail("save setting"); }
}
static int number(const char *text, int maximum) {
    char *end; errno = 0; long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < 0 || value > maximum) {
        fprintf(stderr, "Expected an integer from 0 to %d\n", maximum); exit(2);
    }
    return (int)value;
}
static void save_number(const char *name, const char *text, int maximum) {
    int value = number(text, maximum); char buf[16];
    snprintf(buf, sizeof buf, "%d\n", value); save_text(name, buf);
}
static struct Sound load_sound(const char *path) {
    struct Sound s = {0};
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) fail(path);
    struct stat st; if (fstat(fd, &st)) fail("stat sound");
    if (st.st_size < 44 || st.st_size > 32 * 1024 * 1024) goto bad;
    s.size = (size_t)st.st_size;
    s.map = mmap(NULL, s.size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); fd = -1;
    if (s.map == MAP_FAILED) fail("map sound");
    if (memcmp(s.map, "RIFF", 4) || memcmp(s.map + 8, "WAVE", 4)) goto bad;
    int format_ok = 0;
    for (size_t pos = 12; pos + 8 <= s.size;) {
        size_t len = u32(s.map + pos + 4), body = pos + 8;
        if (len > s.size - body) goto bad;
        if (!memcmp(s.map + pos, "fmt ", 4) && len >= 16)
            format_ok = u16(s.map + body) == 1 && u16(s.map + body + 2) == 1 &&
                        u32(s.map + body + 4) == RATE && u16(s.map + body + 12) == 2 &&
                        u16(s.map + body + 14) == 16;
        if (!memcmp(s.map + pos, "data", 4)) {
            if (len % 2) goto bad;
            s.pcm = s.map + body; s.frames = len / 2;
        }
        pos = body + len + (len & 1);
    }
    if (format_ok && s.pcm && s.frames) return s;
bad:
    fprintf(stderr, "%s must be a valid 44100 Hz mono 16-bit PCM WAV\n", path);
    if (fd >= 0) close(fd);
    exit(1);
}
static void unload(struct Sound *s) { if (s->map) munmap(s->map, s->size); }
static struct Chime load_chimes(const char *loop_path) {
    char dir[PATH_MAX], path[PATH_MAX];
    if (strlen(loop_path) >= sizeof dir) { errno = ENAMETOOLONG; fail("loop path"); }
    strcpy(dir, loop_path); char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0'; else strcpy(dir, ".");
    struct Chime c = {0};
    for (int i = 0; i < 3; i++) {
        char name[48]; snprintf(name, sizeof name, "chimes/quarter-%d.wav", (i + 1) * 15);
        path_join(path, dir, name); c.quarters[i] = load_sound(path);
    }
    path_join(path, dir, "chimes/hour-prefix.wav"); c.prefix = load_sound(path);
    path_join(path, dir, "chimes/strike.wav"); c.strike = load_sound(path);
    return c;
}
static void begin_chime(struct Chime *c, int minute, int hour) {
    c->position = 0; c->active = 1;
    c->quarter = minute ? minute / 15 - 1 : -1;
    c->hits = hour % 12; if (!c->hits) c->hits = 12;
    c->length = minute ? c->quarters[c->quarter].frames :
        c->prefix.frames + (size_t)(c->hits - 1) * STRIKE_SPACING + c->strike.frames;
}
static int chime_sample(const struct Chime *c, size_t frame) {
    if (frame >= c->length) return 0;
    if (c->quarter >= 0) return sample(&c->quarters[c->quarter], frame);
    if (frame < c->prefix.frames) return sample(&c->prefix, frame);
    size_t elapsed = frame - c->prefix.frames; int sum = 0;
    for (int hit = 0; hit < c->hits; hit++) {
        size_t start = (size_t)hit * STRIKE_SPACING;
        if (elapsed >= start && elapsed - start < c->strike.frames)
            sum += sample(&c->strike, elapsed - start);
    }
    return sum;
}
// Search future minute boundaries rather than adding local calendar fields: DST-safe.
static time_t next_quarter(time_t now) {
    time_t next = now - now % 60 + 60;
    for (int i = 0; i < 60; i++, next += 60) {
        struct tm local;
        if (!localtime_r(&next, &local)) fail("local time");
        if (local.tm_min % 15 == 0) return next;
    }
    fputs("Cannot find next quarter\n", stderr); exit(1);
}
static void arm_quarter(int fd, int enabled, time_t *due) {
    struct itimerspec timer = {0};
    *due = enabled ? next_quarter(time(NULL)) : 0;
    timer.it_value.tv_sec = *due;
    if (timerfd_settime(fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &timer, NULL)) fail("arm quarter");
}
static int quarter_is_current(time_t due, time_t now) {
    return due > 0 && now >= due && now - due <= 2;
}
static void preview_chime(struct Chime *c) {
    char path[PATH_MAX]; path_join(path, config_dir, "preview");
    FILE *f = fopen(path, "r"); if (!f) return;
    int minute, hour;
    if (fscanf(f, "%d %d", &minute, &hour) == 2 &&
        (minute == 0 || minute == 15 || minute == 30 || minute == 45) && hour >= 1 && hour <= 12)
        begin_chime(c, minute, hour);
    fclose(f);
}
static int play(const char *path) {
    struct Sound loop = load_sound(path);
    struct Chime chime = load_chimes(path);
    int watcher = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (watcher < 0 || inotify_add_watch(watcher, config_dir,
            IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE) < 0) fail("watch settings");
    int volume = read_setting("volume", 35, 100), previous_volume = -1;
    int chime_volume = read_setting("chime-volume", 35, 100);
    int chime_enabled = read_setting("chime-enabled", 1, 1);
    int timer = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer < 0) fail("quarter timer");
    time_t due; arm_quarter(timer, chime_enabled, &due);
    unsigned char *scaled = malloc(loop.frames * 2);
    if (!scaled) fail("allocate loop");
    int pipefd[2]; if (pipe2(pipefd, O_CLOEXEC)) fail("audio pipe");
    // Bound queued PCM: the default Linux pipe can add almost a second of delay.
    if (fcntl(pipefd[1], F_SETPIPE_SZ, 4096) < 0) fail("audio pipe size");
    pid_t child = fork(); if (child < 0) fail("start audio");
    if (!child) {
        pid_t parent = getppid();
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) || getppid() != parent || parent == 1) _exit(1);
        if (dup2(pipefd[0], STDIN_FILENO) < 0) _exit(1);
        close(pipefd[0]); close(pipefd[1]);
        execlp("pw-cat", "pw-cat", "--playback", "--raw", "--rate", "44100",
               "--channels", "1", "--format", "s16", "--latency", "200ms",
               "--properties", "{ application.name = Ticktack node.name = ticktack media.name = Ticktack }",
               "-", (char *)NULL);
        perror("pw-cat"); _exit(127);
    }
    close(pipefd[0]);
    if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK)) fail("nonblocking audio pipe");
    struct sigaction action = {.sa_handler = on_signal}; sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL); sigaction(SIGINT, &action, NULL); signal(SIGPIPE, SIG_IGN);
    struct pollfd events[] = {{watcher, POLLIN, 0}, {timer, POLLIN, 0}, {pipefd[1], POLLOUT, 0}};
    size_t offset = 0; int result = 0;
    notify_ready();
    while (!stopping) {
        if (volume != previous_volume) {
            for (size_t i = 0; i < loop.frames; i++) {
                uint16_t output = (uint16_t)(int16_t)(sample(&loop, i) * volume / 100);
                scaled[i * 2] = output & 255; scaled[i * 2 + 1] = output >> 8;
            }
            previous_volume = volume;
        }
        if (poll(events, 3, -1) < 0) {
            if (errno == EINTR) continue;
            perror("wait for audio"); result = 1; break;
        }
        if (events[0].revents & POLLIN) {
            union { struct inotify_event align; char data[4096]; } buf;
            ssize_t n;
            while ((n = read(watcher, buf.data, sizeof buf.data)) > 0)
                for (size_t pos = 0; pos < (size_t)n;) {
                    const struct inotify_event *e = (void *)(buf.data + pos);
                    if (e->len && !strcmp(e->name, "preview")) preview_chime(&chime);
                    if ((e->mask & IN_Q_OVERFLOW) || (e->len &&
                        (!strcmp(e->name, "volume") || !strcmp(e->name, "chime-volume") ||
                         !strcmp(e->name, "chime-enabled")))) {
                        volume = read_setting("volume", 35, 100);
                        chime_volume = read_setting("chime-volume", 35, 100);
                        int enabled = read_setting("chime-enabled", 1, 1);
                        if (enabled != chime_enabled) {
                            chime_enabled = enabled;
                            arm_quarter(timer, enabled, &due);
                            if (!enabled) chime.active = 0;
                        }
                    }
                    pos += sizeof *e + e->len;
                }
            if (volume != previous_volume) continue;
        }
        if (events[1].revents & POLLIN) {
            uint64_t expirations; ssize_t n = read(timer, &expirations, sizeof expirations);
            time_t now = time(NULL);
            // No catch-up after suspend or clock changes. Only the current boundary may ring.
            if (n == sizeof expirations && chime_enabled && quarter_is_current(due, now)) {
                struct tm local; localtime_r(&due, &local);
                begin_chime(&chime, local.tm_min, local.tm_hour);
            }
            arm_quarter(timer, chime_enabled, &due);
        }
        if (events[2].revents & (POLLERR | POLLHUP | POLLNVAL)) { result = 1; break; }
        if (events[2].revents & POLLOUT) {
            size_t frames = loop.frames - offset;
            if (frames > 2048) frames = 2048;
            unsigned char mixed[4096]; const unsigned char *output = scaled + offset * 2;
            if (chime.active) {
                for (size_t i = 0; i < frames; i++) {
                    int v = (int16_t)u16(output + i * 2) +
                            chime_sample(&chime, chime.position + i) * chime_volume / 100;
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                    uint16_t value = (uint16_t)(int16_t)v;
                    mixed[i * 2] = value & 255; mixed[i * 2 + 1] = value >> 8;
                }
                output = mixed;
            }
            ssize_t n = write(pipefd[1], output, frames * 2);
            if (n > 0) {
                offset = (offset + (size_t)n / 2) % loop.frames;
                if (chime.active) {
                    chime.position += (size_t)n / 2;
                    if (chime.position >= chime.length) chime.active = 0;
                }
            } else if (n < 0 && errno != EAGAIN && errno != EINTR) { result = 1; break; }
        }
    }
    close(pipefd[1]); close(watcher); close(timer); kill(child, SIGTERM);
    int status; while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    free(scaled); unload(&loop);
    for (int i = 0; i < 3; i++) unload(&chime.quarters[i]);
    unload(&chime.prefix); unload(&chime.strike);
    return result;
}
int main(int argc, char **argv) {
    config_paths();
    const char *setting = NULL; int fallback = 35, maximum = 100;
    if (argc >= 2 && !strcmp(argv[1], "--volume")) setting = "volume";
    if (argc >= 2 && !strcmp(argv[1], "--chime-volume")) setting = "chime-volume";
    if (argc >= 2 && !strcmp(argv[1], "--chime-enabled")) { setting = "chime-enabled"; fallback = maximum = 1; }
    if (setting && argc == 2) { printf("%d\n", read_setting(setting, fallback, maximum)); return 0; }
    if (setting && argc == 3) { save_number(setting, argv[2], maximum); return 0; }
    if (argc == 3 && !strcmp(argv[1], "--play")) return play(argv[2]);
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "--test-chime")) {
        int minute = 0, hour = 3;
        if (!strcmp(argv[2], "hour")) { if (argc == 4) hour = number(argv[3], 12); }
        else if (argc == 3) minute = number(argv[2], 45);
        else return 2;
        if (hour < 1 || (minute != 0 && minute != 15 && minute != 30 && minute != 45) ||
            (minute == 0 && strcmp(argv[2], "hour"))) return 2;
        char buf[32]; snprintf(buf, sizeof buf, "%d %d\n", minute, hour); save_text("preview", buf); return 0;
    }
    fputs("Usage: ticktack-audio --play LOOP.wav | --volume [0..100] |\n"
          "  --chime-volume [0..100] | --chime-enabled [0|1] | --test-chime 15|30|45|hour [1..12]\n", stderr);
    return 2;
}
