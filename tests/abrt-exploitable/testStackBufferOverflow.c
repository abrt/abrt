/*
 * Test a stack buffer overflow
 * This test could be exploitable (it needs further analysis)
 */

#include <stdio.h>

int i;

int my_function() {
    char a[2];

    for (i = 0; i < 1024; i++) {
        a[i] = 'A';
    }
    printf("%s\n", a);
}

int main(int argc, char *argv[]) {
    my_function();
    my_function();
    return 0;
}
