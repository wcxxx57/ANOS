#include "arch/mod.h"
#include "lib/mod.h"

int main()
{
    printf("Hello!This is our OS Kernel!This Kernel is written by %d people:%s and %s.\n \
Let's try and print %c,%p and %x.\n \
We made it!It's amazing!!\n", \
2, "dxy", "wcx", 'A', 0x12345678U, 0x1234567890abcdefULL);
    return 0;
}