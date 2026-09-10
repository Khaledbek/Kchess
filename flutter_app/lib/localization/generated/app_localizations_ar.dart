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
  String get playAgainstBot => 'اللعب ضد بوت';

  @override
  String get playAgainstBotSubtitle => 'ابدأ مباراة محلية ضد Stockfish.';

  @override
  String get continueAgainstBot => 'متابعة اللعب ضد البوت';

  @override
  String get botPlayerColor => 'لونك';

  @override
  String get temporaryBotGameNotSaved =>
      'لن يتم حفظ هذه المباراة وسيتم حذفها بالكامل عند الإغلاق أو الإلغاء.';

  @override
  String get botGameLog => 'سجل مباريات البوت';

  @override
  String get botGameLogSubtitle =>
      'افتح مبارياتك المحفوظة ضد البوت، وتابع المباراة الجارية أو حلّل المباريات المنتهية.';

  @override
  String get botGameLogEmpty => 'لا توجد مباريات بوت محفوظة بعد.';

  @override
  String get botGameLogLoadFailed => 'تعذر تحميل سجل مباريات البوت.';

  @override
  String get botGameHistoryActive => 'جارية';

  @override
  String get botGameHistoryWin => 'فوز';

  @override
  String get botGameHistoryLoss => 'خسارة';

  @override
  String get botGameHistoryDraw => 'تعادل';

  @override
  String get botStrength => 'قوة البوت';

  @override
  String get botStrengthDescription =>
      'اختر قوة البوت بخطوات مقدارها 100 نقطة Elo. يمثل الإعداد الأقصى قوة Stockfish الكاملة.';

  @override
  String get botElo => 'Elo';

  @override
  String get botEngineLockedDescription =>
      'تستخدم مباريات البوت دائمًا Stockfish 18 بشكل مستقل عن محرك التحليل المحدد.';

  @override
  String get botStartGame => 'بدء المباراة';

  @override
  String get botGameTitle => 'مباراة ضد البوت';

  @override
  String get botGameLoading => 'جارٍ تجهيز المباراة …';

  @override
  String get botGameLoadFailed => 'تعذر تجهيز مباراة البوت.';

  @override
  String get botMoveFailed => 'تعذر حساب نقلة البوت.';

  @override
  String get botYou => 'أنت';

  @override
  String get botYourTurn => 'دورك الآن.';

  @override
  String get botThinking => 'Stockfish يفكر …';

  @override
  String get botApplyingMove => 'جارٍ تنفيذ النقلة …';

  @override
  String get botWaiting => 'بانتظار النقلة التالية …';

  @override
  String get botViewingHistory => 'أنت تعرض وضعية سابقة.';

  @override
  String get botGameFinished => 'انتهت المباراة.';

  @override
  String get botMoveList => 'قائمة النقلات';

  @override
  String get botNoMovesYet => 'لم تُلعب أي نقلة بعد.';

  @override
  String get botPreviousMove => 'النقلة السابقة';

  @override
  String get botNextMove => 'النقلة التالية';

  @override
  String get botReturnToLive => 'العودة إلى الوضعية الحالية';

  @override
  String get botHintPiece => 'التلميح 1: إظهار القطعة';

  @override
  String get botHintTarget => 'التلميح 2: إظهار مربع الوصول';

  @override
  String get botHintsUsed => 'تم استخدام التلميحين';

  @override
  String get botHintThinking => 'جارٍ حساب التلميح …';

  @override
  String get botHintFailed => 'تعذر حساب التلميح.';

  @override
  String get botGameSettingsTitle => 'إعدادات المباراة';

  @override
  String get botGameSettingsDescription =>
      'تنطبق هذه الإعدادات على مباراة البوت هذه فقط ولا تغيّر إعدادات التحليل العامة.';

  @override
  String get botGameEvaluationBarHelp =>
      'يعرض تقييم Stockfish 18 الحالي أثناء مباراة البوت هذه. يُحفظ الإعداد مع المباراة.';

  @override
  String botHintCounter(int used) {
    return '$used / 2 تلميحات لهذه النقلة';
  }

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
  String get exportFen => 'تصدير FEN';

  @override
  String get fenCopiedToClipboard => 'تم نسخ FEN إلى الحافظة.';

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
  String get engineSelectionTitle => 'إصدار المحرك';

  @override
  String get engineVersion => 'محرك الشطرنج';

  @override
  String get engineVersionHelp =>
      'اختر إصدار Stockfish الذي يستخدمه KChess للتحليل. تبقى جميع إعدادات التحليل الحالية دون تغيير.';

  @override
  String engineActiveLabel(String engine) {
    return 'النشط: $engine';
  }

  @override
  String get stockfish18 => 'Stockfish 18';

  @override
  String get stockfish19 => 'Stockfish 19';

  @override
  String get engineSelectionFailed =>
      'تعذر تفعيل المحرك المحدد. سيبقى المحرك السابق محددًا.';

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
  String get stockfishPending => 'Stockfish · محلي · GPLv3';

  @override
  String get provider => 'المزود';

  @override
  String get localProfile => 'ملف محلي';

  @override
  String get forced => 'إجباري';

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
  String get good => 'جيدة';

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

  @override
  String get statsWins => 'انتصارات';

  @override
  String get statsDraws => 'تعادلات';

  @override
  String get statsLosses => 'هزائم';

  @override
  String get statsAll => 'الكل';

  @override
  String get statsAllTimeControlsNote => 'كل أنواع الوقت';

  @override
  String get statsPhaseTitle => 'حسب مرحلة اللعب';

  @override
  String get statsPhaseSubtitle =>
      'في أي مرحلة تنتهي مبارياتك وكيف تكون نتيجتك.';

  @override
  String get statsPhaseOpening => 'الافتتاح (1–12)';

  @override
  String get statsPhaseMiddlegame => 'وسط اللعب (13–30)';

  @override
  String get statsPhaseEndgame => 'النهاية (+31)';

  @override
  String get statsPhaseOpeningShort => 'الافتتاح';

  @override
  String get statsPhaseMiddlegameShort => 'وسط اللعب';

  @override
  String get statsPhaseEndgameShort => 'النهاية';

  @override
  String get statsPhaseGames => 'مباراة';

  @override
  String get statsPhaseWinWord => 'فوز';

  @override
  String get statsPhaseEmpty => 'لا توجد بيانات كافية عن مراحل اللعب.';

  @override
  String get statsPhaseNoProfile => 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.';

  @override
  String get statsPhaseError => 'تعذّر تحميل مراحل اللعب.';

  @override
  String get statsPhaseRetry => 'إعادة المحاولة';

  @override
  String statsPhaseClassifiedNote(int classified, int total) {
    return '$classified من $total مباراة';
  }

  @override
  String get statsTitle => 'الإحصائيات';

  @override
  String get statsIntroTitle => 'أداؤك في الشطرنج';

  @override
  String get statsIntroBody =>
      'اعرض نتائجك وأداءك الأخير وسجل افتتاحياتك مفصولًا حسب اللون.';

  @override
  String get statsFormTitle => 'الأداء الأخير';

  @override
  String get statsFormHint => 'اضغط على نتيجة لفتح المباراة.';

  @override
  String get statsFormVersus => 'ضد';

  @override
  String get statsFormEmpty => 'لا توجد مباريات حديثة لعرضها.';

  @override
  String get statsFormError => 'تعذّر تحميل المباريات الأخيرة.';

  @override
  String get statsFormRetry => 'إعادة المحاولة';

  @override
  String get statsOverviewTitle => 'نظرة عامة';

  @override
  String get statsOverviewGames => 'المباريات';

  @override
  String get statsOverviewWinRate => 'نسبة الفوز';

  @override
  String get statsOverviewScore => 'النتيجة';

  @override
  String get statsOverviewRecord => 'السجل';

  @override
  String get statsOverviewByColor => 'حسب اللون';

  @override
  String get statsOverviewByTimeControl => 'حسب نوع الوقت';

  @override
  String get statsOverviewWhite => 'أبيض';

  @override
  String get statsOverviewBlack => 'أسود';

  @override
  String get statsOverviewEmpty =>
      'لا توجد مباريات بعد. زامِن حسابًا على الإنترنت أو استورد مباريات لعرض إحصاءاتك.';

  @override
  String get statsOverviewNoProfile =>
      'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.';

  @override
  String get statsOverviewNoGamesForFilter =>
      'لا توجد مباريات لنوع الوقت المحدد.';

  @override
  String get statsOverviewError => 'تعذّر تحميل الإحصاءات.';

  @override
  String get statsOverviewRetry => 'إعادة المحاولة';

  @override
  String get statsTerminationTitle => 'طريقة انتهاء المباريات';

  @override
  String get statsTerminationCheckmate => 'كش ملك';

  @override
  String get statsTerminationResignation => 'استسلام';

  @override
  String get statsTerminationTimeout => 'انتهاء الوقت';

  @override
  String get statsTerminationDraw => 'تعادل';

  @override
  String get statsTerminationOther => 'أخرى';

  @override
  String get statsTerminationWonByCheckmate => 'فوز بكش ملك';

  @override
  String get statsTerminationLostByCheckmate => 'خسارة بكش ملك';

  @override
  String get statsTerminationOpponentResigned => 'استسلم الخصم';

  @override
  String get statsTerminationSelfResigned => 'استسلمت';

  @override
  String get statsTerminationOpponentFlagged => 'نفد وقت الخصم';

  @override
  String get statsTerminationSelfFlagged => 'نفد وقتك';

  @override
  String get statsTerminationWonGeneric => 'فوز';

  @override
  String get statsTerminationLostGeneric => 'خسارة';

  @override
  String get statsTerminationEmpty =>
      'لا توجد بيانات كافية عن نهايات المباريات.';

  @override
  String get statsTerminationNoProfile =>
      'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.';

  @override
  String get statsTerminationError => 'تعذّر تحميل نهايات المباريات.';

  @override
  String get statsTerminationRetry => 'إعادة المحاولة';

  @override
  String get statsRatingTitle => 'تطوّر التصنيف';

  @override
  String get statsRatingEmpty => 'لا توجد بيانات تصنيف كافية لرسم منحنى.';

  @override
  String get statsRatingError => 'تعذّر تحميل بيانات التصنيف.';

  @override
  String get statsRatingRetry => 'إعادة المحاولة';

  @override
  String get statsOpeningGamesAll => 'الكل';

  @override
  String get statsOpeningGamesWon => 'فوز';

  @override
  String get statsOpeningGamesLost => 'خسارة';

  @override
  String get statsOpeningGamesByCheckmate => 'بكش ملك';

  @override
  String get statsOpeningGamesByResignation => 'بالاستسلام';

  @override
  String get statsOpeningGamesByTimeout => 'بانتهاء الوقت';

  @override
  String get statsOpeningGamesByDraw => 'تعادل';

  @override
  String get statsOpeningGamesEmpty => 'لا توجد مباريات لهذه الاختيار.';

  @override
  String get statsOpeningGamesError => 'تعذّر تحميل المباريات.';

  @override
  String get statsOpeningsTitle => 'أنجح الافتتاحيات';

  @override
  String get statsOpeningsMostPlayed => 'الأكثر لعبًا';

  @override
  String get statsOpeningsBestWinRate => 'أفضل نسبة فوز';

  @override
  String get statsOpeningsMinGamesHint => '3 مباريات على الأقل لكل افتتاحية.';

  @override
  String get statsOpeningsClassifiedGames => 'مباراة بافتتاحية معروفة';

  @override
  String get statsOpeningsGames => 'مباراة';

  @override
  String get statsOpeningsVariations => 'تنويعات';

  @override
  String get statsOpeningsBaseLine => 'الشكل الأساسي';

  @override
  String get statsOpeningsWhite => 'الأبيض';

  @override
  String get statsOpeningsBlack => 'الأسود';

  @override
  String get statsOpeningsUnknownColor => 'أخرى';

  @override
  String get statsOpeningsWinRateShort => 'فوز';

  @override
  String get statsOpeningsNoOpeningsForColor => 'لا توجد افتتاحيات لهذا اللون.';

  @override
  String get statsOpeningsNoOpeningsForWinRate =>
      'لا توجد افتتاحية بثلاث مباريات على الأقل.';

  @override
  String get statsOpeningsEmpty =>
      'لا توجد افتتاحيات مُصنّفة بعد. تُصنَّف المباريات المستوردة والمتزامنة تلقائيًا.';

  @override
  String get statsOpeningsNoProfile =>
      'أنشئ أو اختر ملفًا شخصيًا لعرض الافتتاحيات.';

  @override
  String get statsOpeningsError => 'تعذّر تحميل الافتتاحيات.';

  @override
  String get statsOpeningsRetry => 'إعادة المحاولة';

  @override
  String get statsCompareTitle => 'مقارنة اللاعبين';

  @override
  String get statsCompareUsernameLabel => 'اسم مستخدم Chess.com';

  @override
  String get statsCompareUsernameHint => 'مثال: hikaru';

  @override
  String get statsCompareCompare => 'قارن';

  @override
  String get statsCompareLoadingHint => 'يتم جلب مباريات الخصم وتحليلها…';

  @override
  String get statsComparePrompt =>
      'أدخل اسم مستخدم Chess.com لمقارنة الإحصاءات.';

  @override
  String get statsCompareYou => 'أنت';

  @override
  String get statsCompareOpponent => 'الخصم';

  @override
  String get statsCompareH2hTitle => 'المواجهات المباشرة';

  @override
  String get statsCompareDirectGames => 'مباريات مباشرة';

  @override
  String get statsCompareWins => 'فوز';

  @override
  String get statsCompareDraws => 'تعادل';

  @override
  String get statsCompareLosses => 'خسارة';

  @override
  String get statsComparePerformanceCompare => 'مقارنة الأداء';

  @override
  String get statsCompareWinRateWhite => 'نسبة الفوز بالأبيض';

  @override
  String get statsCompareWinRateBlack => 'نسبة الفوز بالأسود';

  @override
  String get statsCompareFlagging => 'الخسارة بانتهاء الوقت';

  @override
  String get statsCompareOpeningMatchup => 'مواجهات الافتتاحيات';

  @override
  String get statsCompareMatchupSubtitle =>
      'افتتاحياتك مقابل نسبة فوز الخصم باللون المقابل.';

  @override
  String get statsCompareOpeningColumn => 'الافتتاحية';

  @override
  String get statsCompareGamesShort => 'مباراة';

  @override
  String get statsCompareNoMatchups => 'لا توجد افتتاحيات مشتركة.';

  @override
  String get statsCompareNoLeaks => 'لا توجد نقاط ضعف واضحة.';

  @override
  String get statsCompareStrategyTitle => 'الاستراتيجية المقترحة';

  @override
  String get statsCompareColorWhite => 'الأبيض';

  @override
  String get statsCompareColorBlack => 'الأسود';

  @override
  String get statsCompareErrorPrefix => 'خطأ';

  @override
  String statsCompareGamesAnalyzed(int games, int months) {
    return 'تم تحليل $games مباراة من $months أشهر';
  }

  @override
  String statsTerminationSpotlight(String label, int share, int lossPercent) {
    return 'أكثر نهاية شيوعًا: $label — $share% من كل المباريات، منها $lossPercent% خسائر.';
  }

  @override
  String get statsCompareSelfBadge => 'مقارنة ذاتية (انعكاس)';

  @override
  String get statsCompareSelfH2H =>
      'لا توجد مباريات ضد نفسك · هذا تحليل ذاتي لملفك الشخصي.';

  @override
  String statsCompareScopeNote(int games) {
    return 'تعتمد المقارنة على آخر $games مباراة تم تحميلها.';
  }

  @override
  String get statsCompareMinSampleNote =>
      'فقط الافتتاحيات بخمس مباريات على الأقل لكل طرف.';

  @override
  String get statsCompareOwnWeaknessTitle => 'نقاط ضعفك';

  @override
  String statsCompareOwnWeakness(
    String color,
    String opening,
    String rate,
    int games,
  ) {
    return 'نقطة ضعف بالـ$color: $opening — نسبة فوز $rate فقط (من $games مباراة).';
  }

  @override
  String get statsCompareNoOwnWeakness =>
      'لا توجد نقاط ضعف واضحة بخمس مباريات على الأقل لكل افتتاحية.';

  @override
  String statsCompareOpenWhite(String opening, String rate, int games) {
    return 'افتتح بـ$opening — يحقق خصمك بالأسود ضدها $rate فقط (من $games مباراة).';
  }

  @override
  String statsCompareAnswerBlack(String opening, String rate, int games) {
    return 'بالأسود: اختر $opening — يحقق خصمك بالأبيض ضدها $rate فقط (من $games مباراة).';
  }

  @override
  String statsCompareSampleScope(int months, int games) {
    return 'أنت: مكتبتك المحلية بالكامل · الخصم: آخر $months أشهر ($games مباراة). الجانبان عيّنتان مختلفتان، لذا قد تختلف القيم حتى عند مقارنة الملف بنفسه.';
  }

  @override
  String get statsTimeControlCorrespondence => 'بالمراسلة';

  @override
  String get statsTimeControlOther => 'أخرى';

  @override
  String get botGameDeleteQuestion => 'حذف مباراة البوت؟';

  @override
  String get botGameDeleteBody =>
      'سيتم حذف هذا الإدخال من سجل مباريات البوت نهائيًا. ستبقى أي مباراة تحليل تم إنشاؤها منه في مكتبة الألعاب المحلية.';

  @override
  String get promotionTitle => 'ترقية البيدق';

  @override
  String get promotionChoosePiece =>
      'اختر القطعة التي تريد ترقية البيدق إليها.';

  @override
  String get promotionQueen => 'وزير';

  @override
  String get promotionRook => 'قلعة';

  @override
  String get promotionBishop => 'فيل';

  @override
  String get promotionKnight => 'حصان';

  @override
  String get trainingSection => 'التدريب';

  @override
  String get trainingIntroTitle => 'ساحة التدريب';

  @override
  String get trainingIntroBody =>
      'اختر مجال تدريب. يُحفظ تقدمك محليًا على هذا الجهاز.';

  @override
  String get trainingOpeningTitle => 'مختبر الافتتاحيات';

  @override
  String get trainingOpeningSubtitle => 'راجع خطوط ذخيرتك ونقاط ضعفك.';

  @override
  String get trainingOpeningAction => 'درّب الخطوط';

  @override
  String trainingNemesisBadge(String opening, String rate) {
    return 'أصعب افتتاحية: $opening — نسبة فوز $rate فقط';
  }

  @override
  String get trainingNemesisNone => 'لم يتم رصد نقطة ضعف في إحصاءاتك بعد.';

  @override
  String get trainingTacticsTitle => 'صائد الأخطاء';

  @override
  String get trainingTacticsSubtitle =>
      'ابحث عن النقلة الأفضل في أوضاع وسط اللعب الحرجة.';

  @override
  String trainingTacticsSolved(int count) {
    return '$count تكتيكًا تم حله';
  }

  @override
  String get trainingTacticsAction => 'ابدأ التكتيك';

  @override
  String get trainingTacticsEmpty => 'لا توجد ألغاز تكتيكية في الفهرس بعد.';

  @override
  String get trainingEndgameTitle => 'أكاديمية النهايات';

  @override
  String get trainingEndgameSubtitle => 'أتقن النهايات النظرية خطوة بخطوة.';

  @override
  String trainingEndgameProgress(int mastered, int total, int percent) {
    return '$mastered / $total وضعية تم إتقانها ($percent%)';
  }

  @override
  String get trainingEndgameAction => 'افتح النهايات';

  @override
  String get trainingMastered => 'متقن';

  @override
  String trainingStreak(int done, int total) {
    return '$done/$total تكرارات بلا خطأ';
  }

  @override
  String trainingLastAttempt(String date) {
    return 'آخر تدريب: $date';
  }

  @override
  String get trainingNeverAttempted => 'لم يتم التدريب بعد';

  @override
  String get trainingOpeningLabSelected => 'الخط المختار';

  @override
  String get trainingOpeningLabEmpty =>
      'اختر افتتاحية من تبويب الإحصاءات واضغط «تدريب» لتحميلها هنا.';

  @override
  String get trainingOpeningLabPending =>
      'سيتوفر مدرّب الخطوط بمجرد أن توفّر النواة نقلات الافتتاحية.';

  @override
  String get statsTrainOpening => 'تدريب';

  @override
  String trainingGoalWin(String side) {
    return 'الدور على $side — اربح الوضعية';
  }

  @override
  String trainingGoalDraw(String side) {
    return 'الدور على $side — حافظ على التعادل';
  }

  @override
  String get trainingWrongMove => 'ليست أفضل نقلة. حاول مرة أخرى.';

  @override
  String get trainingSolvedTitle => 'ممتاز! تم حل الوضعية.';

  @override
  String get trainingSolvedClean => 'حُلّت دون خطأ — تُحتسب ضمن الإتقان.';

  @override
  String get trainingSolvedWithErrors =>
      'حُلّت لكن بتصحيحات. لا يُحتسب للإتقان إلا الأداء الخالي من الأخطاء.';

  @override
  String get trainingPracticeAgain => 'تدرّب مجددًا';

  @override
  String get trainingNextEndgame => 'النهاية التالية';

  @override
  String get trainingBackToList => 'إلى القائمة';

  @override
  String trainingMoveProgress(int done, int total) {
    return 'النقلة $done من $total';
  }

  @override
  String get trainingBoardError => 'تعذّر تحميل الوضعية.';

  @override
  String get trainingShowHint => 'أظهر التلميح';

  @override
  String get trainingYourMove => 'دورك';

  @override
  String get trainingRestart => 'إعادة البدء';

  @override
  String get trainingOppositionTitle => 'تقابل الملكين مع بيدق';

  @override
  String get trainingOppositionHint =>
      'احصل على التقابل أولًا ثم التف حول الملك. لا تحرك البيدق قبل أن يصبح ملكك أمامه.';

  @override
  String get trainingLucenaTitle => 'وضعية لوسينا';

  @override
  String get trainingLucenaHint =>
      'ضع الرخ على الصف الرابع قبل إخراج الملك ليصبح جسرًا يحميه من الكشوف.';

  @override
  String get trainingPhilidorTitle => 'دفاع فيليدور';

  @override
  String get trainingPhilidorHint =>
      'أبقِ الرخ على الصف السادس حتى يتقدم البيدق، ثم أعطِ الكش من الخلف.';
}
