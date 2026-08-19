import 'package:flutter/material.dart';

import 'app/kchess_app.dart';
import 'ffi/ffi_core_gateway.dart';
import 'features/app/application/app_controller.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final gateway = await FfiCoreGateway.create();
  runApp(KChessApp(controller: AppController(gateway)));
}
