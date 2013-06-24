/*
 * Test the illegal instruction signal
 * This error is exploitable
 */

#include <signal.h>

int main(int argc, char *argv[]) {
    raise(SIGILL);
}
