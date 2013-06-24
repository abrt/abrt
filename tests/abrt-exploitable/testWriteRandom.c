/*
 * Test a crash attempting to write invalid memory
 * This error is exploitable
 */

#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char *a;
    char b[] = "pwnt";
    a = malloc(1024);
    a = (size_t)a * 1024;   // This should put us well outside the valid memory range
    strcpy(a, b);
}
