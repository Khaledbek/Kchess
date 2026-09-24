import chess.polyglot

def generate_header():
    array = chess.polyglot.POLYGLOT_RANDOM_ARRAY
    print('#include "polyglot.h"')
    print('#include <cstdint>')
    print('namespace kchess {')
    print('const std::uint64_t PolyglotRandomArray[781] = {')
    for i in range(0, len(array), 4):
        chunk = array[i:i+4]
        print('    ' + ', '.join(f'{x}ULL' for x in chunk) + ',')
    print('};')
    print('}')
    
generate_header()
