# Development Guide

## Quick Setup

**Prerequisites**: Python 3.8+ with pip, [ufbt](https://pypi.org/project/ufbt/) (micro Flipper Build Tool), Git

`ufbt` builds this repo standalone — it downloads the Flipper SDK/toolchain on first run, so you never need to clone the full firmware repo or nest this app inside `applications_user/`.

**Setup**:
```bash
pip install ufbt
git clone https://github.com/YOUR_USERNAME/flipper-wallet-app.git
cd flipper-wallet-app
```

By default `ufbt` targets the official Flipper SDK. To build against the Unleashed SDK instead:
```bash
ufbt update --index-url=https://up.unleashedflip.com/directory.json --channel=dev
```
Re-run this after every `ufbt update`, otherwise it silently falls back to the official SDK.

## Building

**Build**:
```bash
ufbt
# Output: dist/wallet_app.fap
```

**Install to Device**:
```bash
ufbt launch      # build, install and run on a connected Flipper
# or:
ufbt flash_usb   # build and flash over USB
```
Both require a serial connection (`/dev/ttyACM*` or COM port). Without one, copy `dist/wallet_app.fap` to `/data/apps/Tools/` manually.

**VS Code Integration**:
```bash
ufbt vscode_dist   # generates .vscode configuration for building/debugging
```

## Code Structure

**Data Types**:
- `WalletFieldType` - Enum (String, Number, Date)
- `WalletField` - Label + type
- `WalletCategory` - Name + fields
- `WalletEntry` - Field values
- `EntryListModel` - List view model

**Key Functions**:
- Config: `wallet_app_load_config()`, `wallet_app_parse_fields()`
- Storage: `wallet_app_load_entries()`, `wallet_app_save_entries()`
- UI: `wallet_app_begin_field_edit()`, `wallet_app_entry_list_draw_callback()`
- Navigation: `wallet_app_navigation_callback()` (state machine)
- Lifecycle: `wallet_app_alloc()`, `wallet_app_free()`, `wallet_app_main()`

## Adding a New Field Type

Example: Add Phone type

1. Update enum `WalletFieldType`: add `WalletFieldTypePhone`
2. Update `wallet_app_parse_field_type()`: add `if(strcmp(type_str, "Phone") == 0) return WalletFieldTypePhone;`
3. Add `WalletViewPhoneInput` to `WalletView` enum
4. Add `TextInput* phone_input;` to `WalletApp` struct
5. Initialize in `wallet_app_alloc()`: `app->phone_input = text_input_alloc();`
6. Handle in `wallet_app_begin_field_edit()`: dispatch to phone input widget
7. Cleanup in `wallet_app_free()`: free phone_input

See [README.md](README.md) development section for full example.

## Common Issues

| Issue | Solution |
|-------|----------|
| "undefined reference" | Check `requires=["gui", "storage"]` in application.fam |
| FAP won't load / API mismatch | Run `ufbt update` to match your Flipper's firmware, verify size <100KB |
| Compilation errors | Use safe functions: `strlcpy()`, `strlcat()` |
| Config not loading | Verify format exactly matches spec, check `/data/wallet/wallet.conf` |
| `ufbt update` reverts to official SDK | Re-run with `--index-url=https://up.unleashedflip.com/directory.json --channel=dev` |

## Resources

- [ufbt on PyPI](https://pypi.org/project/ufbt/)
- [Flipper Zero Docs](https://docs.flipperzero.io/)
- [Unleashed Firmware](https://github.com/DarkFlippers/unleashed-firmware)
