/*
 * Test a NULL read
 * This error is not exploitable
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
    char *a;
    a = 0x0;
    puts(a);
}
