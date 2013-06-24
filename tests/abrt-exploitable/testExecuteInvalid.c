/*
 * Test a crash attempting to execute an invalid address
 * This error is exploitable
 */

#include <stdlib.h>
#include <string.h>

int (*function_pointer)();

int main(int argc, char *argv[]) {
    char *a;
    a = malloc(1024);
    a = (size_t)a * 1024;   // This should put us well outside the valid memory range
    function_pointer = a;
    function_pointer();
}
