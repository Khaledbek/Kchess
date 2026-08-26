part of '../../../ui/app_root.dart';

class _DepthRangeSettingTile extends StatelessWidget {
  const _DepthRangeSettingTile({
    required this.title,
    required this.description,
    required this.minimumDepth,
    required this.maximumDepth,
    required this.onMinimumChanged,
    required this.onMaximumChanged,
    super.key,
  });

  final String title;
  final String description;
  final int minimumDepth;
  final int maximumDepth;
  final Future<void> Function(int value) onMinimumChanged;
  final Future<void> Function(int value) onMaximumChanged;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      child: Row(
        children: [
          const Icon(Icons.account_tree_outlined),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title, style: Theme.of(context).textTheme.bodyLarge),
                const SizedBox(height: 2),
                Text(
                  description,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: Theme.of(context).colorScheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          Flexible(
            child: FittedBox(
              alignment: Alignment.centerRight,
              fit: BoxFit.scaleDown,
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  _CompactIntegerControl(
                    key: const Key('engine-min-depth'),
                    label: 'Min',
                    value: minimumDepth,
                    minimum: 1,
                    maximum: maximumDepth,
                    onChanged: onMinimumChanged,
                  ),
                  const SizedBox(width: 12),
                  _CompactIntegerControl(
                    key: const Key('engine-max-depth'),
                    label: 'Max',
                    value: maximumDepth,
                    minimum: minimumDepth,
                    maximum: 64,
                    onChanged: onMaximumChanged,
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _CompactIntegerControl extends StatefulWidget {
  const _CompactIntegerControl({
    required this.label,
    required this.value,
    required this.minimum,
    required this.maximum,
    required this.onChanged,
    super.key,
  });

  final String label;
  final int value;
  final int minimum;
  final int maximum;
  final Future<void> Function(int value) onChanged;

  @override
  State<_CompactIntegerControl> createState() => _CompactIntegerControlState();
}

class _CompactIntegerControlState extends State<_CompactIntegerControl> {
  late int _value;
  int _pendingWrites = 0;
  Future<void> _writeQueue = Future<void>.value();

  @override
  void initState() {
    super.initState();
    _value = widget.value;
  }

  @override
  void didUpdateWidget(covariant _CompactIntegerControl oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (_pendingWrites == 0 && widget.value != _value) {
      _value = widget.value;
    }
    if (_value < widget.minimum) _value = widget.minimum;
    if (_value > widget.maximum) _value = widget.maximum;
  }

  void _changeValue(int next) {
    final bounded = next.clamp(widget.minimum, widget.maximum).toInt();
    if (bounded == _value) return;
    setState(() {
      _value = bounded;
      _pendingWrites += 1;
    });
    final previousWrite = _writeQueue;
    _writeQueue = () async {
      try {
        await previousWrite;
      } catch (_) {}
      await widget.onChanged(bounded);
    }();
    _writeQueue.then(
      (_) => _finishWrite(),
      onError: (Object _, StackTrace __) => _finishWrite(),
    );
  }

  void _finishWrite() {
    if (!mounted) return;
    setState(() {
      if (_pendingWrites > 0) _pendingWrites -= 1;
      if (_pendingWrites == 0) _value = widget.value;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(widget.label, style: Theme.of(context).textTheme.labelMedium),
        const SizedBox(width: 4),
        IconButton(
          visualDensity: VisualDensity.compact,
          constraints: const BoxConstraints(minWidth: 34, minHeight: 34),
          padding: EdgeInsets.zero,
          onPressed: _value > widget.minimum
              ? () => _changeValue(_value - 1)
              : null,
          icon: const Icon(Icons.remove, size: 18),
        ),
        SizedBox(
          width: 44,
          child: TextFormField(
            key: ValueKey('${widget.key}-$_value'),
            initialValue: '$_value',
            textAlign: TextAlign.center,
            keyboardType: TextInputType.number,
            textInputAction: TextInputAction.done,
            decoration: const InputDecoration(
              isDense: true,
              contentPadding: EdgeInsets.symmetric(vertical: 8, horizontal: 4),
            ),
            onFieldSubmitted: (text) {
              final parsed = int.tryParse(text);
              if (parsed != null) _changeValue(parsed);
            },
          ),
        ),
        IconButton(
          visualDensity: VisualDensity.compact,
          constraints: const BoxConstraints(minWidth: 34, minHeight: 34),
          padding: EdgeInsets.zero,
          onPressed: _value < widget.maximum
              ? () => _changeValue(_value + 1)
              : null,
          icon: const Icon(Icons.add, size: 18),
        ),
      ],
    );
  }
}

class _IntegerSettingTile extends StatefulWidget {
  const _IntegerSettingTile({
    required this.icon,
    required this.title,
    required this.value,
    required this.minimum,
    required this.maximum,
    required this.onChanged,
    this.description,
    this.valueLabel,
    this.valueLabelBuilder,
    this.allowedValues,
    this.editable = false,
    super.key,
  });

  final IconData icon;
  final String title;
  final String? description;
  final int value;
  final int minimum;
  final int maximum;
  final String? valueLabel;
  final String Function(int value)? valueLabelBuilder;
  final List<int>? allowedValues;
  final bool editable;
  final Future<void> Function(int value) onChanged;

  @override
  State<_IntegerSettingTile> createState() => _IntegerSettingTileState();
}

class _IntegerSettingTileState extends State<_IntegerSettingTile> {
  late int _value;
  int _pendingWrites = 0;
  Future<void> _writeQueue = Future<void>.value();

  @override
  void initState() {
    super.initState();
    _value = widget.value;
  }

  @override
  void didUpdateWidget(covariant _IntegerSettingTile oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (_pendingWrites == 0 && widget.value != _value) {
      _value = widget.value;
    }
  }

  int? get _previousValue {
    final values = widget.allowedValues;
    if (values == null) {
      return _value > widget.minimum ? _value - 1 : null;
    }
    final index = values.indexOf(_value);
    if (index > 0) return values[index - 1];
    if (index == -1) {
      final lower = values.where((candidate) => candidate < _value).toList();
      return lower.isEmpty ? null : lower.last;
    }
    return null;
  }

  int? get _nextValue {
    final values = widget.allowedValues;
    if (values == null) {
      return _value < widget.maximum ? _value + 1 : null;
    }
    final index = values.indexOf(_value);
    if (index >= 0 && index < values.length - 1) return values[index + 1];
    if (index == -1) {
      final higher = values.where((candidate) => candidate > _value).toList();
      return higher.isEmpty ? null : higher.first;
    }
    return null;
  }

  void _changeValue(int next) {
    if (next == _value) return;
    setState(() {
      _value = next;
      _pendingWrites += 1;
    });

    final previousWrite = _writeQueue;
    _writeQueue = () async {
      try {
        await previousWrite;
      } catch (_) {
        // A failed earlier write must not block later user input.
      }
      await widget.onChanged(next);
    }();

    _writeQueue.then(
      (_) => _finishWrite(),
      onError: (Object _, StackTrace __) => _finishWrite(),
    );
  }

  void _finishWrite() {
    if (!mounted) return;
    setState(() {
      if (_pendingWrites > 0) _pendingWrites -= 1;
      if (_pendingWrites == 0) {
        _value = widget.value;
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final previous = _previousValue;
    final next = _nextValue;
    final range = '${widget.minimum}–${widget.maximum}';
    final label =
        widget.valueLabelBuilder?.call(_value) ??
        widget.valueLabel ??
        '$_value';
    return ListTile(
      leading: Icon(widget.icon),
      title: Text(widget.title),
      subtitle: widget.description == null
          ? Text(range)
          : Text('${widget.description}\n$range'),
      isThreeLine: widget.description != null,
      trailing: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          IconButton(
            onPressed: previous == null ? null : () => _changeValue(previous),
            icon: const Icon(Icons.remove),
          ),
          SizedBox(
            width: 72,
            child: widget.editable
                ? TextFormField(
                    key: ValueKey('integer-${widget.key}-$_value'),
                    initialValue: '$_value',
                    textAlign: TextAlign.center,
                    keyboardType: TextInputType.number,
                    textInputAction: TextInputAction.done,
                    decoration: const InputDecoration(isDense: true),
                    onFieldSubmitted: (text) {
                      final parsed = int.tryParse(text);
                      if (parsed != null) {
                        final bounded = parsed < widget.minimum
                            ? widget.minimum
                            : (parsed > widget.maximum
                                  ? widget.maximum
                                  : parsed);
                        _changeValue(bounded);
                      }
                    },
                  )
                : Text(
                    label,
                    textAlign: TextAlign.center,
                    textDirection: TextDirection.ltr,
                  ),
          ),
          IconButton(
            onPressed: next == null ? null : () => _changeValue(next),
            icon: const Icon(Icons.add),
          ),
        ],
      ),
    );
  }
}

