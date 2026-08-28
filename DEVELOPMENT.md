# Development Guide

## Quick Setup

**Prerequisites**: Flipper Zero firmware dev environment, FBT, C99 compiler

**Setup**:
```bash
# Clone firmware
git clone https://github.com/DarkFlippers/unleashed-firmware.git
cd unleashed-firmware

# Add app
cp -r /path/to/flipper-wallet-app applications_user/wallet_app
```

## Building

**Build**:
```bash
fbt fap_wallet_app
# Output: build/f7-firmware-D/.extapps/wallet_app.fap
```

**Install to Device**:
1. Connect Flipper to computer
2. Copy `.fap` to `/data/apps/Tools/`
3. Restart or refresh Applications menu

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
| FAP won't load | Verify size <100KB, check API version |
| Compilation errors | Use safe functions: `strlcpy()`, `strlcat()` |
| Config not loading | Verify format exactly matches spec, check `/data/wallet/wallet.conf` |

## Resources

- [Flipper Zero Docs](https://docs.flipperzero.io/)
- [Unleashed Firmware](https://github.com/DarkFlippers/unleashed-firmware)
- [FBT Documentation](https://docs.flipperzero.io/development/documentation/fbt)
