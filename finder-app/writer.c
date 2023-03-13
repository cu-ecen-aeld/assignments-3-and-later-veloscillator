#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    openlog("writerapp", LOG_PERROR, LOG_USER);

    bool success = false;
    int fd = -1;

    // Validation.
    if (argc != 3) {
        syslog(LOG_ERR, "must specify writefile and writestr");
        goto exit;
    }
    const char* writefile = argv[1];
    const char* writestr = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    fd = open(
        writefile,
        O_CREAT | O_EXCL | O_WRONLY,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        syslog(LOG_ERR, "error creating file: %s", strerror(errno));
        goto exit;
    }

    const size_t bytes_total = strlen(writestr);
    size_t bytes_written = 0;
    while (bytes_written < bytes_total) {
        ssize_t result = write(fd, writestr + bytes_written, bytes_total - bytes_written);
        if (result == -1) {
            syslog(LOG_ERR, "failed to write: %s", strerror(errno));
            goto exit;
        }
        bytes_written += (size_t)result;
    }

    success = true;

exit:
    if (fd != -1) {
        close(fd);
    }
    closelog();
    return success ? 0 : 1;
}
