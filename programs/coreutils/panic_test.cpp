#include <stdio.h>

int main() {
    printf("Memicu BSOD secara instan...\n");
    // Cara paling cepat bikin kernel marah: akses alamat memori nol (0)
    volatile int* p = (volatile int*)0;
    *p = 0xDEADBEEF; 
    return 0;
}