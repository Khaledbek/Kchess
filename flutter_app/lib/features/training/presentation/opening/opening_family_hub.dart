// -----------------------------------------------------------------------------
// Section: Scenarios below one opening family
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../../ffi/core_gateway.dart';
import '../../models/practice_models.dart';
import 'opening_scenarios.dart';

/// The variations of one opening, each a drillable scenario of its own. The
/// family itself is drilled from its card on the list this was opened from.
class OpeningFamilyHubScreen extends StatelessWidget {
  const OpeningFamilyHubScreen({
    required this.gateway,
    required this.rootNode,
    required this.color,
    super.key,
  });

  final CoreGateway gateway;
  final OpeningTreeNode rootNode;
  final String color;

  @override
  Widget build(BuildContext context) => OpeningScenarioPage(
    title: rootNode.name,
    body: OpeningScenarioList(
      gateway: gateway,
      parent: rootNode.id,
      color: color,
    ),
  );
}
