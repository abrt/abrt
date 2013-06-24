/*
 * Test a crash attempting to read invalid memory
 * This error is not exploitable
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char *a;
    a = malloc(1024);
    a = (size_t)a * 1024;   // This should put us well outside the valid memory range
    printf("%s\n", a);
}
