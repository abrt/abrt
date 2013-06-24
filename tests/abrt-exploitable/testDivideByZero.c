/*
 * Test a divide by zero error
 * This error is not exploitable
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("%d\n", 7/0);
}
