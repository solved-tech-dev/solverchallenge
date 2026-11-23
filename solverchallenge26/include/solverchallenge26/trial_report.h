#ifndef SOLVERCHALLENGE26_TRIAL_REPORT_H
#define SOLVERCHALLENGE26_TRIAL_REPORT_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline void sc26_report_trial(int trial, double elapsed_seconds)
{
    char line[96];
    int length = snprintf(
        line, sizeof(line),
        "TRIAL trial=%d raw_seconds=%.12f\n",
        trial, elapsed_seconds);
    if (length <= 0)
        return;

    size_t bytes = (size_t)length < sizeof(line)
        ? (size_t)length
        : sizeof(line) - 1;
    (void)write(STDERR_FILENO, line, bytes);

    const char *path = getenv("SC26_TRIAL_LOG");
    if (path != NULL && path[0] != '\0') {
        int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd >= 0) {
            (void)write(fd, line, bytes);
            (void)close(fd);
        }
    }
}

#endif
