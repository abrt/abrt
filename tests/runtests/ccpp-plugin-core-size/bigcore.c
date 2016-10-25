#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <err.h>

int main(int argc, char *argv[])
{
    long bufsize = 1024*1024;
    long loops = 0;
    int opt;

    while ((opt = getopt(argc, argv, "M:")) != -1) {
        switch (opt) {
            case 'M':
                loops = atoi(optarg);
                break;
            default:
                errx(EXIT_FAILURE, "Usage: %s [-M MEGA]", argv[0]);

        }
    }

    if (loops == 0) {
        errx(EXIT_FAILURE, "Usage: %s [-M MEGA]", argv[0]);
    }

    for (int i = 0; i < loops; ++i) {
        uint8_t *buf = (uint8_t *)malloc(bufsize * sizeof(uint8_t));

        if (buf == NULL) {
            err(EXIT_FAILURE, "malloc");
        }
    }

    abort();

    /* Dead code! */
    exit(EXIT_FAILURE);
}
