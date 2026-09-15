// -----------------------------------------------------------------------------
// Section: Pawn promotion choice
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../../localization/generated/app_localizations.dart';

Future<String?> showPromotionChoiceDialog({
  required BuildContext context,
  required List<String> options,
  required String sideToMove,
}) {
  final strings = AppLocalizations.of(context);
  final white = sideToMove == 'white';
  final assets = <String, String>{
    'q': white
        ? 'assets/analysis_img/piece_white_queen.svg'
        : 'assets/analysis_img/piece_black_queen.svg',
    'r': white
        ? 'assets/analysis_img/piece_white_rook.svg'
        : 'assets/analysis_img/piece_black_rook.svg',
    'b': white
        ? 'assets/analysis_img/piece_white_bishop.svg'
        : 'assets/analysis_img/piece_black_bishop.svg',
    'n': white
        ? 'assets/analysis_img/piece_white_knight.svg'
        : 'assets/analysis_img/piece_black_knight.svg',
  };
  final labels = <String, String>{
    'q': strings.promotionQueen,
    'r': strings.promotionRook,
    'b': strings.promotionBishop,
    'n': strings.promotionKnight,
  };

  return showDialog<String>(
    context: context,
    builder: (dialogContext) => AlertDialog(
      title: Text(strings.promotionTitle),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(strings.promotionChoosePiece),
          const SizedBox(height: 16),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              for (final option in options)
                if (assets.containsKey(option))
                  Tooltip(
                    message: labels[option] ?? option,
                    child: InkWell(
                      key: Key('promotion-choice-$option'),
                      borderRadius: BorderRadius.circular(10),
                      onTap: () => Navigator.of(dialogContext).pop(option),
                      child: SizedBox.square(
                        dimension: 64,
                        child: Padding(
                          padding: const EdgeInsets.all(7),
                          child: SvgPicture.asset(assets[option]!),
                        ),
                      ),
                    ),
                  ),
            ],
          ),
        ],
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(dialogContext).pop(),
          child: Text(strings.close),
        ),
      ],
    ),
  );
}
