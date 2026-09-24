import 'package:flutter/material.dart';

import 'app/kchess_app.dart';
import 'diagnostics/app_startup_diagnostics.dart';
import 'ffi/ffi_core_gateway.dart';
import 'features/app/application/app_controller.dart';

Future<void> main() async {
  final startup = AppStartupDiagnostics.instance;
  startup.startMain();
  WidgetsFlutterBinding.ensureInitialized();
  startup.mark('bindingReadyMs');
  final gateway = await FfiCoreGateway.create();
  startup.mark('ffiGatewayReadyMs');
  startup.mark('runAppCalledMs');
  runApp(KChessApp(controller: AppController(gateway)));
}
