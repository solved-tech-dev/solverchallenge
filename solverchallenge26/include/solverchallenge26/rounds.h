#ifndef SC26_ROUNDS_H
#define SC26_ROUNDS_H

#include <stdio.h>
#include <stdlib.h>

/* The shell workflow starts a fresh process for each numbered round. */
static int sc26_first_round(void)
{
    const char* value = getenv("SC26_TRIAL_INDEX");
    char* end;
    long trial;

    if (!value) {
        return 0;
    }
    trial = strtol(value, &end, 10);
    if (end == value || *end != '\0' || trial < 0 || trial >= 10) {
        fprintf(stderr, "SC26_TRIAL_INDEX must be 0 through 9\n");
        exit(EXIT_FAILURE);
    }
    return (int)trial;
}

static int sc26_round_limit(void)
{
    return getenv("SC26_TRIAL_INDEX") ? sc26_first_round() + 1 : 10;
}

#endif
