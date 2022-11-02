#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
    // @note The BYTE_ORDER define seems to be set to BIG_ENDIAN on my Intel i5,
    // so the following check will have incorrect results: #if BYTE_ORDER == BIG_ENDIAN
    // Also it seems wise not to use those checks anyway, and write endian transparent portable code instead:
    // https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
    // https://justine.lol/endian.html
    
    static inline uint64_t swapBytesIfHostIsLittleEndian(uint64_t value) {
        // Operators (such as bit shifts) in C operate on the mathematical value, regardless of byte order
        // The right shift always shifts towards the least significant byte (so it's more like a 'down' shift)
        // Assigning a larger unsigned type to a smaller unsigned type (e.g. uint64_t to uint8_t) truncates the high order bits preserving the least significant bits
        // (this is different for signed types)
        // So with those tools, we can write code that results in a swap only if the host is little endian
        
        // The example below uses a uint32_t value of 0x7ff06040
        // and an example operation of: data[1] = value >> 16; to take the second byte
        
        // Little endian memory starts at the least significant byte
        //                 LSB                        MSB
        // Host LE:       [1000000 11000000 11100000 11111111]
        //                 ^<---------------^ Shift the whole thing 16 bits toward the LSB (to the left in this graphic)
        // After shift:   [11100000 11111111 00000000 00000000]
        // Assigning to a byte results in [11100000] (takes the least significant bits)
        // 
        // Host LE:       [1000000 11000000 11100000 11111111]
        // [0] = >> 24:                               11111111
        // [1] = >> 16:                      11100000
        // [2] = >> 8:              11000000
        // [3] = >> 0:     1000000
        // data:          [11111111 11100000 11000000 1000000]
        
        // Big endian memory starts at the most significant byte
        //                 MSB                        LSB
        // Host BE:       [11111111 11100000 11000000 1000000]
        //                          ^---------------->^ Shift the whole thing 16 bits toward the LSB (to the right in this graphic)
        // After shift:   [00000000 00000000 11111111 11100000]
        // Assigning to a byte results in [11100000] (takes the least significant bits)
        // 
        // Host BE:       [11111111 11100000 11000000 1000000]
        // [0] = >> 24:    11111111
        // [1] = >> 16:             11100000
        // [2] = >> 8:                       11000000
        // [3] = >> 0:                                1000000
        // data:          [11111111 11100000 11000000 1000000]
        
        // Note that we don't need to mask any remaining bits e.g.
        // data[0] = (0xff00000000000000 & value) >> 56;
        // or
        // data[0] = (value >> 56) & 0xff;
        // Because we assign to a uint8_t that can only hold a single byte (and the assignment discards the other bits for us)
        // If we were to assign to a larger type, we would need to mask
        
        // --------------------------------------------------------------------
        
        // For completeness we'll show the same example, but with bytes coming from the network needing conversion to the host byte order
        // Network byte order is big endian, so it needs swapping on little endian hosts
        // Network value: [11111111 11100000 11000000 1000000]
        
        //                 LSB                        MSB
        // Host LE:       [11111111 11100000 11000000 1000000]
        //                 ^<----------------^ Shift the whole thing 16 bits toward the LSB
        // After shift:   [11000000 1000000 00000000 00000000]
        // Assigning to a byte results in [11000000]
        // 
        // Host LE:       [11111111 11100000 11000000 1000000]
        // [0] = >> 24:                               1000000
        // [1] = >> 16:                      11000000
        // [2] = >> 8:              11100000
        // [3] = >> 0:     11111111
        // data:          [1000000 11000000 11100000 11111111]
        
        //                 MSB                        LSB
        // Host BE:       [11111111 11100000 11000000 1000000]
        //                          ^---------------->^ Shift the whole thing 16 bits toward the LSB
        // After shift:   [00000000 00000000 11111111 11100000]
        // Assigning to a byte results in [11100000]
        // 
        // Host BE:       [11111111 11100000 11000000 1000000]
        // [0] = >> 24:    11111111
        // [1] = >> 16:             11100000
        // [2] = >> 8:                       11000000
        // [3] = >> 0:                                1000000
        // data:          [11111111 11100000 11000000 1000000]
        
        uint64_t result;
        // Treat result as bytes
        uint8_t *data = (uint8_t *)&result;
        data[0] = value >> 56;
        data[1] = value >> 48;
        data[2] = value >> 40;
        data[3] = value >> 32;
        data[4] = value >> 24;
        data[5] = value >> 16;
        data[6] = value >> 8;
        data[7] = value >> 0;
        
        return result;
    }
    
    // a.k.a. htobe64
    uint64_t htonll(uint64_t value) {
        return swapBytesIfHostIsLittleEndian(value);
    }
    
    // a.k.a. be64toh
    uint64_t ntohll(uint64_t value) {
        return swapBytesIfHostIsLittleEndian(value);
    }
#endif