// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Arabic (`ar`).
class AppLocalizationsAr extends AppLocalizations {
  AppLocalizationsAr([String locale = 'ar']) : super(locale);

  @override
  String get appTitle => 'KChess';

  @override
  String get firstRunTitle => 'مساحة الشطرنج المحلية';

  @override
  String get firstRunBody =>
      'اختر مصدرًا. الملفات العامة على الإنترنت لا تحتاج إلى كلمة مرور.';

  @override
  String get chessCom => 'Chess.com';

  @override
  String get lichess => 'Lichess';

  @override
  String get localPgnFen => 'PGN / FEN';

  @override
  String get username => 'اسم المستخدم';

  @override
  String get profileName => 'اسم الملف الشخصي';

  @override
  String get continueLabel => 'متابعة';

  @override
  String get requiredField => 'يرجى إدخال قيمة.';

  @override
  String get games => 'المباريات';

  @override
  String get gameSection => 'المباراة';

  @override
  String get play => 'اللعب';

  @override
  String get playPlaceholder =>
      'هذا القسم محجوز كعنصر نائب لأنماط لعب مستقبلية، مثل اللعب ضد البوتات.';

  @override
  String get downloads => 'التنزيلات';

  @override
  String get favorites => 'المفضلة';

  @override
  String get favoriteCollectionsTitle => 'المجموعات';

  @override
  String get favoriteNoCollections =>
      'لا توجد مجموعات بعد. أنشئ مجموعة لتنظيم مبارياتك المفضلة.';

  @override
  String get favoriteLooseTitle => 'مفضلة بدون مجموعة';

  @override
  String get favoriteCreateCollection => 'إنشاء مجموعة';

  @override
  String get favoriteRenameCollection => 'إعادة تسمية المجموعة';

  @override
  String get favoriteDeleteCollection => 'حذف المجموعة';

  @override
  String get favoriteCollectionName => 'اسم المجموعة';

  @override
  String get favoriteDeleteCollectionBody =>
      'سيتم حذف المجموعة، وستبقى مبارياتها ضمن المفضلة بدون مجموعة.';

  @override
  String get favoriteEmptyCollection =>
      'لا تحتوي هذه المجموعة على مباريات بعد.';

  @override
  String get favoriteNoLooseGames => 'لا توجد مباريات مفضلة بدون مجموعة.';

  @override
  String get favoriteCollectionRule =>
      'المجموعات بمستوى واحد فقط ولا يمكن إنشاء مجموعات داخلها.';

  @override
  String get favoriteMoveToCollection => 'تغيير المجموعة';

  @override
  String get favoriteMoveHelp =>
      'يمكن أن تبقى المباراة ضمن المفضلة بدون مجموعة أو تنتمي إلى مجموعة واحدة فقط.';

  @override
  String get profile => 'الملف الشخصي';

  @override
  String get settings => 'الإعدادات';

  @override
  String get analysis => 'التحليل';

  @override
  String get addAccount => 'إضافة حساب';

  @override
  String get switchAccount => 'تبديل الحساب';

  @override
  String get demoNotice => 'مباراة محلية';

  @override
  String get tapToAnalyze => 'فتح وتحليل';

  @override
  String get importData => 'استيراد PGN / FEN';

  @override
  String get importPgnFile => 'اختيار ملف PGN';

  @override
  String get pastePgn => 'لصق نص PGN';

  @override
  String get importFen => 'استيراد وضعية FEN';

  @override
  String get pgnText => 'نص PGN';

  @override
  String get pgnLabel => 'PGN المباراة';

  @override
  String get fenText => 'FEN كامل';

  @override
  String get positionName => 'اسم الوضعية';

  @override
  String get importAction => 'استيراد';

  @override
  String get noGames =>
      'لا توجد مباريات أو وضعيات محلية بعد. استورد ملف PGN أو نص PGN أو وضعية FEN.';

  @override
  String get emptySection => 'هذا القسم جاهز للبيانات المحلية.';

  @override
  String get loading => 'جارٍ تحميل النواة المحلية…';

  @override
  String get coreUnavailable => 'تعذر تشغيل النواة المحلية.';

  @override
  String get retry => 'إعادة المحاولة';

  @override
  String get summary => 'الملخص';

  @override
  String get analyzing => 'جارٍ تحليل كل نقلة نصفية…';

  @override
  String get analysisComplete => 'اكتمل التحليل الكامل';

  @override
  String get analysisCancelled =>
      'تم إلغاء التحليل — بقيت النتائج الموجودة محفوظة.';

  @override
  String get cancelAnalysis => 'إلغاء التحليل';

  @override
  String get deleteAnalysis => 'حذف التحليل المحفوظ';

  @override
  String get deleteAnalysisQuestion => 'حذف التحليل المحفوظ؟';

  @override
  String get deleteAnalysisBody =>
      'سيتم حذف التحليل المحلي المحفوظ ودقة هذه المباراة. سيبقى PGN/FEN وذاكرة المحرك العامة.';

  @override
  String get analysisDeleted => 'تم حذف التحليل المحفوظ.';

  @override
  String get classificationPending => 'تصنيف هذه النقلة غير متاح بعد.';

  @override
  String get bestMove => 'أفضل نقلة';

  @override
  String get evaluation => 'التقييم';

  @override
  String get engineLines => 'خطوط المحرك';

  @override
  String get currentMove => 'النقلة الحالية';

  @override
  String get boardArrows => 'إظهار أسهم الرقعة';

  @override
  String get boardArrowsHelp => 'للعرض فقط؛ التبديل لا يعيد التحليل.';

  @override
  String get engine => 'المحرك';

  @override
  String get enginePreset => 'متوسط · عمق 18 · 3 خطوط';

  @override
  String get depth => 'العمق';

  @override
  String get numberOfLines => 'عدد الخطوط';

  @override
  String get timeLimitSeconds => 'الحد الزمني (بالثواني)';

  @override
  String get noTimeLimit => 'متوقف';

  @override
  String get secondsShort => 'ث';

  @override
  String get deleteAccount => 'حذف الحساب';

  @override
  String get deleteAccountQuestion => 'حذف الحساب؟';

  @override
  String get deleteOnlineProfileBody =>
      'سيُزال هذا الملف الشخصي وبياناته المحفوظة محليًا من KChess. لن يتغير حساب Chess.com أو Lichess نفسه.';

  @override
  String get deleteLocalProfileBody =>
      'سيُزال هذا الملف الشخصي وبيانات PGN/FEN المحفوظة محليًا من KChess.';

  @override
  String get cancelAction => 'إلغاء';

  @override
  String get deleteAction => 'حذف';

  @override
  String get language => 'اللغة';

  @override
  String get theme => 'المظهر';

  @override
  String get systemTheme => 'النظام';

  @override
  String get lightTheme => 'فاتح';

  @override
  String get darkTheme => 'داكن';

  @override
  String get engineSettingsSubtitle =>
      'العمق والخطوط والحد الزمني وخيوط المعالجة وذاكرة Hash';

  @override
  String get analysisSettingsTitle => 'التحليل';

  @override
  String get analysisSettingsSubtitle => 'الأسهم والتقييم وطريقة عرض التحليل';

  @override
  String get analysisBoardGuidance => 'إرشادات الرقعة';

  @override
  String get analysisInformation => 'معلومات التحليل';

  @override
  String get bestMoveArrow => 'سهم أفضل نقلة';

  @override
  String get bestMoveArrowHelp => 'يعرض أفضل نقلة يقترحها المحرك على الرقعة.';

  @override
  String get threatArrow => 'سهم التهديد';

  @override
  String get threatArrowHelp =>
      'يعرض أقوى نقلة تالية للخصم كسهم تحذير عندما يكون الدور على الخصم.';

  @override
  String get evaluationBarSetting => 'شريط التقييم';

  @override
  String get evaluationBarSettingHelp => 'يعرض تقييم المحرك الحالي.';

  @override
  String get showEngineLinesSetting => 'إظهار خطوط المحرك';

  @override
  String get showEngineLinesSettingHelp =>
      'يعرض الخطوط الرئيسية المحسوبة (MultiPV).';

  @override
  String get showClassificationsSetting => 'إظهار تصنيفات النقلات';

  @override
  String get showClassificationsSettingHelp =>
      'يعرض النظرية والبارعة والحاسمة والأفضل وبقية تصنيفات النقلات.';

  @override
  String get showAccuracySetting => 'إظهار الدقة';

  @override
  String get showAccuracySettingHelp => 'يعرض قيم الدقة المحسوبة محليًا.';

  @override
  String get showTheorySetting => 'إظهار معلومات النظرية';

  @override
  String get showTheorySettingHelp =>
      'يعرض معلومات كتاب الافتتاح وعدّادات النقلات النظرية.';

  @override
  String get showResultSymbolsSetting => 'إظهار رموز النتيجة';

  @override
  String get showResultSymbolsSettingHelp =>
      'يعرض رموز الفوز أو الخسارة أو التعادل فوق الملكين عند انتهاء المباراة.';

  @override
  String get designSettingsTitle => 'التصميم';

  @override
  String get designSettingsSubtitle => 'المظهر والسمة والرقعة والقطع';

  @override
  String get generalSettingsTitle => 'عام';

  @override
  String get generalSettingsSubtitle => 'اللغة وسلوك التطبيق';

  @override
  String get dataStorageSettingsTitle => 'البيانات والتخزين';

  @override
  String get dataStorageSettingsSubtitle =>
      'ذاكرة التحليل والتنزيلات والبيانات المحلية';

  @override
  String get dataStoragePlaceholder =>
      'ستُضاف خيارات التخزين وذاكرة التخزين المؤقت في خطوة لاحقة.';

  @override
  String get licensesAbout => 'التراخيص وحول التطبيق';

  @override
  String get stockfishPending => 'Stockfish 18 · محلي · GPLv3';

  @override
  String get provider => 'المزود';

  @override
  String get localProfile => 'ملف محلي';

  @override
  String get theory => 'النظرية';

  @override
  String get brilliant => 'بارعة';

  @override
  String get critical => 'نقلة رائعة';

  @override
  String get best => 'الأفضل';

  @override
  String get excellent => 'ممتازة';

  @override
  String get okay => 'جيدة';

  @override
  String get miss => 'فرصة ضائعة';

  @override
  String get mistake => 'خطأ';

  @override
  String get blunder => 'خطأ فادح';

  @override
  String get totalMoves => 'أنصاف النقلات';

  @override
  String get localAccuracy => 'الدقة المحلية';

  @override
  String get close => 'إغلاق';

  @override
  String get previous => 'السابق';

  @override
  String get next => 'التالي';

  @override
  String get first => 'الأولى';

  @override
  String get last => 'الأخيرة';

  @override
  String get playPause => 'تشغيل أو إيقاف';

  @override
  String get player => 'اللاعب';

  @override
  String get bothPlayers => 'كلاهما';

  @override
  String get whitePlayer => 'الأبيض';

  @override
  String get blackPlayer => 'الأسود';

  @override
  String get analyzedMoves => 'النقلات المصنفة';

  @override
  String get bookGames => 'مباريات الافتتاح';

  @override
  String get expectedLoss => 'خسارة التوقع';

  @override
  String get versions => 'الإصدارات';

  @override
  String get classifierVersionLabel => 'المصنّف';

  @override
  String get accuracyVersionLabel => 'الدقة';

  @override
  String get analyzingGame => 'جارٍ تحليل المباراة';

  @override
  String analyzedMovesProgress(int completed, int total) {
    return 'تم تحليل $completed / $total نصف نقلة';
  }

  @override
  String get openAnalysis => 'فتح التحليل';

  @override
  String bestMoveText(String move) {
    return '$move هي أفضل نقلة.';
  }

  @override
  String moveComparisonText(String played, String classification, String best) {
    return 'كانت $played مصنفة: $classification. ‏$best هي أفضل نقلة.';
  }

  @override
  String theoryMoveText(String move) {
    return '$move نقلة نظرية.';
  }

  @override
  String triedMove(String move) {
    return 'جرّبت $move.';
  }

  @override
  String get sidelineEngineTitle => 'محرك الخط الجانبي';

  @override
  String get sidelineEngineSubtitle =>
      'تُطبَّق هذه القيم فقط على التحليل المباشر لخطك الجانبي.';

  @override
  String get mainLineLabel => 'الخط الرئيسي';

  @override
  String get sidelineLabel => 'خطك البديل';

  @override
  String get liveEngineTheorySkipped => 'نظرية: تم تجاوز التحليل المباشر';

  @override
  String get liveEngineTargetReached => 'Stockfish: تم بلوغ هدف التحليل';

  @override
  String liveEngineProgress(int percent) {
    return 'Stockfish يحلل مباشرة · $percent%';
  }

  @override
  String get sidelineAnalysisPaused => 'تم إيقاف التحليل المباشر مؤقتًا';

  @override
  String get analyzingVariation => 'جارٍ تحليل الخط المؤقت…';

  @override
  String evaluationComparison(String before, String after) {
    return 'التقييم: $before ← $after';
  }

  @override
  String bestContinuation(String line) {
    return 'أفضل متابعة: $line';
  }

  @override
  String get returnToMainLine => 'العودة إلى الخط الرئيسي';

  @override
  String illegalOrFailedMove(String message) {
    return 'النقلة غير قانونية أو تعذر تحليلها: $message';
  }

  @override
  String get myPlayer => 'لاعبي';

  @override
  String get opponent => 'الخصم';

  @override
  String get variationStartingPosition => 'وضع بداية الخط';

  @override
  String get variationStart => 'بداية الخط';

  @override
  String get engineQualityTitle => 'جودة التحليل';

  @override
  String get engineResourcesTitle => 'الموارد';

  @override
  String get depthHelp =>
      'Min = عمق التحليل المسبق. Max = أقصى عمق للتحليل المباشر. القيم الأعلى تستغرق عادة وقتًا أطول.';

  @override
  String get adaptiveEarlyStop => 'تحليل تكيفي';

  @override
  String get adaptiveEarlyStopHelp =>
      'ينهي التحليل المسبق والتحليل المباشر الهادئ مبكرًا عندما يثبت التقييم وخطوط اللعب الرئيسية. تستمر عمليات التحقق الحرجة حتى الحد المحدد.';

  @override
  String get numberOfLinesHelp =>
      'عدد أفضل الخطوط التي يحسبها Stockfish في الوقت نفسه.';

  @override
  String get timeLimitHelp =>
      'حد اختياري لكل وضعية. عند إيقافه يُستخدم العمق فقط، وإلا يتوقف البحث عند بلوغ العمق أو الوقت أولًا.';

  @override
  String get threads => 'خيوط المعالجة';

  @override
  String get threadsHelp =>
      'عدد خيوط المعالج لكل عامل Stockfish. يكتشف Kchess جهازك تلقائيًا ويسمح بحد أقصى بنصف خيوط المعالج المنطقية.';

  @override
  String get hashMemory => 'ذاكرة Hash';

  @override
  String get hashMemoryHelp =>
      'ذاكرة RAM لجدول النقل في Stockfish. زيادة الذاكرة قد تحسن البحث في الوضعيات المتكررة.';

  @override
  String get boardDisplayTitle => 'عرض الرقعة';

  @override
  String get rotateBoard => 'تدوير الرقعة';

  @override
  String get showBoardCoordinates => 'إحداثيات الرقعة';

  @override
  String get showBoardCoordinatesHelp =>
      'يعرض أسماء الأعمدة والصفوف (a–h / 1–8) على الرقعة.';

  @override
  String get highlightLastMove => 'تمييز آخر نقلة';

  @override
  String get highlightLastMoveHelp =>
      'يميز مربع البداية ومربع النهاية لآخر نقلة تم لعبها.';

  @override
  String get highlightSelectedSquare => 'تمييز المربع المحدد';

  @override
  String get highlightSelectedSquareHelp =>
      'يميز المربع الذي اخترته أثناء استكشاف تفريع.';

  @override
  String get behaviorTitle => 'السلوك';

  @override
  String get autoSyncOnline => 'مزامنة الحسابات عبر الإنترنت تلقائيًا';

  @override
  String get autoSyncOnlineHelp =>
      'يزامن Chess.com وLichess تلقائيًا عند بدء التطبيق وعند تبديل الحساب.';

  @override
  String get confirmBeforeDelete => 'التأكيد قبل الحذف';

  @override
  String get confirmBeforeDeleteHelp =>
      'يطلب التأكيد قبل حذف الحسابات أو المباريات المحلية.';

  @override
  String get analysisCacheTitle => 'ذاكرة التحليل المؤقتة';

  @override
  String get useGlobalAnalysisCache => 'استخدام ذاكرة المواقف المشتركة';

  @override
  String get useGlobalAnalysisCacheHelp =>
      'يعيد استخدام تحليل متوافق للمواقف المتطابقة بين مباريات مختلفة.';

  @override
  String get clearAnalysisCache => 'مسح ذاكرة التحليل';

  @override
  String get clearAnalysisCacheHelp =>
      'يمسح ذاكرة المواقف المشتركة فقط. تبقى المباريات المحفوظة والتحليلات المكتملة.';

  @override
  String get clearAnalysisCacheQuestion => 'مسح ذاكرة التحليل؟';

  @override
  String get clearAnalysisCacheBody =>
      'سيتم حذف ذاكرة المواقف المشتركة. ستبقى مبارياتك والمفضلة والتنزيلات والتحليلات المكتملة.';

  @override
  String get analysisCacheCleared => 'تم مسح ذاكرة التحليل.';

  @override
  String get diagnosticsTitle => 'التشخيص';

  @override
  String get diagnosticLogging => 'سجل التشخيص';

  @override
  String get diagnosticLoggingHelp =>
      'يكتب سجلات تقنية محدودة لاستكشاف الأخطاء. لا يتم تسجيل PGN أو FEN الكامل أو ردود مزودي الخدمة.';

  @override
  String get deleteLocalGameQuestion => 'حذف الإدخال المحلي؟';

  @override
  String get deleteLocalGameBody =>
      'سيتم حذف PGN/FEN المحفوظ وتحليله المحلي نهائيًا.';

  @override
  String get profileRatings => 'التصنيفات';

  @override
  String get profileGameOverview => 'نظرة عامة على المباريات';

  @override
  String get profileWins => 'انتصارات';

  @override
  String get profileDraws => 'تعادلات';

  @override
  String get profileLosses => 'خسائر';

  @override
  String get ratingRapid => 'سريع';

  @override
  String get ratingBlitz => 'خاطف';

  @override
  String get ratingBullet => 'رصاصة';

  @override
  String get ratingDaily => 'يومي';

  @override
  String get ratingClassical => 'كلاسيكي';

  @override
  String get ratingChess960 => 'شطرنج 960';

  @override
  String get ratingFide => 'FIDE';
}
