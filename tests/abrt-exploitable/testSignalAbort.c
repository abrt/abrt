/*
 * Test the abort signal
 * This error is not exploitable
 */

#include <signal.h>

int main(int argc, char *argv[]) {
    raise(SIGABRT);
}
