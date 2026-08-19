import 'dart:ffi';

import 'package:flutter_test/flutter_test.dart';

typedef _SmokeNative = Int32 Function(Int32);
typedef _SmokeDart = int Function(int);

void main() {
  const libraryPath = String.fromEnvironment('KCHESS_CORE_PATH');

  test(
    'compiled C ABI smoke test returns through dart:ffi',
    () {
      final library = DynamicLibrary.open(libraryPath);
      final smoke = library.lookupFunction<_SmokeNative, _SmokeDart>(
        'kc_smoke_test',
      );
      expect(smoke(41), 42);
    },
    skip: libraryPath.isEmpty
        ? 'Set KCHESS_CORE_PATH to the compiled native library.'
        : false,
  );
}
