#include "SimpleCache.h"

int main() {

    uint32_t addresses[3];

    // Assign the addresses to the array
    addresses[0] = 0x00000000;  // Address 1 maps to set 0 and has tag 0
    addresses[1] = 0x00004000;  // Address 2 maps to set 0 and has tag 1
    addresses[2] = 0x00008000;  // Address 3 maps to set 0 and has tag 2

    uint32_t tag_mask = 0xFFFFC000;
    uint32_t index_mask = 0x0003FC0;
    int clock1, value;
    int address = 0;  // We will use the same address for all operations
    //int num_repeats = 10; // Number of times we repeat the read/write
    
    printf("\nTesting repeated accesses to the same address...\n\n");

    // Initialize the cache before the first run
    initCache();

    for (int i = 0; i < 3; i++) {
        // Reset time for each iteration
        resetTime();

        // Write to the same address
        write(addresses[i], (unsigned char *)(&address));
        uint32_t tag = (addresses[i] & tag_mask) >> 14;
        uint32_t set = (address & index_mask) >> 6;
        clock1 = getTime();
        printf("Write; Set: %d Block Tag: %d Address %d; Value %d; Time %d\n", set, tag, addresses[i], value, clock1);

        resetTime();

        // Read from the same address
        read(addresses[i], (unsigned char *)(&value));
        clock1 = getTime();
        printf("Read; Set: %d, Block Tag: %d Address %d; Value %d; Time %d\n", set, tag, addresses[i], value, clock1);
    }
    
    resetTime();

    write(addresses[0], (unsigned char *)(&address));
    uint32_t tag = (addresses[0] & tag_mask) >> 14;
    uint32_t set = (address & index_mask) >> 6;
    clock1 = getTime();
    printf("Write; Set: %d Block Tag: %d Address %d; Value %d; Time %d\n", set, tag, addresses[0], value, clock1);

    return 0;
}
