part of '../app_root.dart';

class FirstRunScreen extends StatefulWidget {
  const FirstRunScreen({required this.controller, super.key});

  final AppController controller;

  @override
  State<FirstRunScreen> createState() => _FirstRunScreenState();
}

class _FirstRunScreenState extends State<FirstRunScreen> {
  final _formKey = GlobalKey<FormState>();
  final _input = TextEditingController();
  ProfileType _type = ProfileType.chessCom;
  bool _saving = false;

  @override
  void dispose() {
    _input.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    if (!_formKey.currentState!.validate()) {
      return;
    }
    setState(() => _saving = true);
    try {
      await widget.controller.createProfile(_type, _input.text);
    } catch (error) {
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    } finally {
      if (mounted) {
        setState(() => _saving = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    return Scaffold(
      body: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: AppTheme.ambientGradient(theme.brightness),
          ),
        ),
        child: SafeArea(
          child: Center(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(24),
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 560),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const BrandLogo(size: 88),
                    const SizedBox(height: 18),
                    const BrandWordmark(fontSize: 32),
                    const SizedBox(height: 28),
                    Card(
                      child: Padding(
                        padding: const EdgeInsets.all(24),
                        child: Form(
                          key: _formKey,
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              Text(
                                strings.firstRunTitle,
                                textAlign: TextAlign.center,
                                style: theme.textTheme.headlineSmall,
                              ),
                              const SizedBox(height: 8),
                              Text(
                                strings.firstRunBody,
                                textAlign: TextAlign.center,
                                style: theme.textTheme.bodyMedium?.copyWith(
                                  color: theme.colorScheme.onSurfaceVariant,
                                ),
                              ),
                              const SizedBox(height: 26),
                              SegmentedButton<ProfileType>(
                                key: const Key('profile-type-selector'),
                                showSelectedIcon: false,
                                segments: [
                                  ButtonSegment(
                                    value: ProfileType.chessCom,
                                    icon: const Icon(Icons.public),
                                    label: Text(strings.chessCom),
                                  ),
                                  ButtonSegment(
                                    value: ProfileType.lichess,
                                    icon: const Icon(Icons.language),
                                    label: Text(strings.lichess),
                                  ),
                                  ButtonSegment(
                                    value: ProfileType.localPgnFen,
                                    icon: const Icon(Icons.upload_file),
                                    label: Text(strings.localPgnFen),
                                  ),
                                ],
                                selected: {_type},
                                onSelectionChanged: (selection) => setState(() {
                                  _type = selection.first;
                                  _input.clear();
                                }),
                              ),
                              const SizedBox(height: 20),
                              TextFormField(
                                key: const Key('profile-input'),
                                controller: _input,
                                textInputAction: TextInputAction.done,
                                onFieldSubmitted: (_) => _submit(),
                                decoration: InputDecoration(
                                  prefixIcon: Icon(
                                    _type == ProfileType.localPgnFen
                                        ? Icons.badge_outlined
                                        : Icons.alternate_email,
                                  ),
                                  labelText: _type == ProfileType.localPgnFen
                                      ? strings.profileName
                                      : strings.username,
                                ),
                                validator: (value) =>
                                    value == null || value.trim().isEmpty
                                    ? strings.requiredField
                                    : null,
                              ),
                              const SizedBox(height: 18),
                              FilledButton(
                                key: const Key('create-profile'),
                                onPressed: _saving ? null : _submit,
                                child: _saving
                                    ? const SizedBox.square(
                                        dimension: 20,
                                        child: CircularProgressIndicator(
                                          strokeWidth: 2,
                                        ),
                                      )
                                    : Text(strings.continueLabel),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

