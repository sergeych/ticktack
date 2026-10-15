// SPDX-License-Identifier: CC-BY-4.0
// Copyright (c) 2026 sergeych and Ticktack contributors
#define main ticktack_program_main
#include "../ticktack.c"
#undef main
#include <assert.h>

static time_t utc(int year, int month, int day, int hour, int minute, int second) {
    struct tm tm = {.tm_year=year-1900, .tm_mon=month-1, .tm_mday=day,
                    .tm_hour=hour, .tm_min=minute, .tm_sec=second};
    return timegm(&tm);
}
int main(void) {
    setenv("TZ", "UTC", 1); tzset();
    assert(next_quarter(utc(2026,10,15,12,14,59)) == utc(2026,10,15,12,15,0));
    assert(next_quarter(utc(2026,10,15,12,15,0)) == utc(2026,10,15,12,30,0));
    assert(next_quarter(utc(2026,10,15,23,59,59)) == utc(2026,10,16,0,0,0));
    setenv("TZ", "America/New_York", 1); tzset();
    assert(next_quarter(utc(2026,11,1,5,59,59)) == utc(2026,11,1,6,0,0));
    assert(next_quarter(utc(2026,3,8,6,59,59)) == utc(2026,3,8,7,0,0));
    setenv("TZ", "Asia/Kathmandu", 1); tzset();
    assert(next_quarter(utc(2026,10,15,12,14,59)) == utc(2026,10,15,12,15,0));
    assert(!quarter_is_current(100, 99));
    assert(quarter_is_current(100, 100));
    assert(quarter_is_current(100, 102));
    assert(!quarter_is_current(100, 103));
    assert(!quarter_is_current(100, 10000));
    assert(!quarter_is_current(0, 0));

    struct Chime c = load_chimes("sounds/04-nightcustard-aligned-loop-12s.wav");
    struct Sound tick = load_sound("sounds/04-nightcustard-aligned-loop-12s.wav");
    for (int minute = 15; minute <= 45; minute += 15) {
        begin_chime(&c, minute, 5);
        assert(c.quarter == minute/15-1 && c.position == 0);
        assert(c.length == c.quarters[minute/15-1].frames);
        assert(chime_sample(&c, c.length) == 0);
    }
    for (int hour = 0; hour < 24; hour++) {
        begin_chime(&c, 0, hour);
        assert(c.hits == (hour%12 ? hour%12 : 12));
        assert(c.length == c.prefix.frames + (size_t)(c.hits-1)*STRIKE_SPACING+c.strike.frames);
    }
    begin_chime(&c, 0, 2);
    size_t probe = c.prefix.frames + STRIKE_SPACING + 1000;
    assert(chime_sample(&c, probe) == sample(&c.strike, STRIKE_SPACING+1000)+sample(&c.strike,1000));
    begin_chime(&c, 0, 12);
    int peak = 0;
    for (size_t i = 0; i < c.length; i++) {
        int mixed = sample(&tick, i%tick.frames)*35/100 + chime_sample(&c,i)*35/100;
        if (abs(mixed) > peak) peak=abs(mixed);
    }
    assert(peak < 32767);
    printf("PASS: quarter boundaries, midnight, DST, no catch-up; 1..12 strikes; overlapping tails; default mix peak %d/32767\n",peak);
    unload(&tick); unload(&c.prefix); unload(&c.strike);
    for(int i=0;i<3;i++)unload(&c.quarters[i]);
    return 0;
}
