#include "arch/mod.h"
#include "lib/mod.h"

int main()
{
    printf("Hello OS Kernel!\n");
    printf("Integer: %d\n", -123);
    printf("Hex32: %p\n", 0xdeadbeefU);
    printf("Hex64: %x\n", 0x1a2b3c4d5e6f7080ULL);
    printf("Char: %c, String: %s\n", 'K', "Kernel Mode");
    printf("Null string: %s\n", (char*)0);
    return 0;
}