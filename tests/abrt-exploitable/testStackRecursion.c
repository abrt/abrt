/*
 * Test a crash from stack recursion
 * This error is not exploitable
 */

#include <stdio.h>

void my_function() {
    char a[1024];
    my_function();
}

int main(int argc, char *argv[]) {
    my_function();
}
