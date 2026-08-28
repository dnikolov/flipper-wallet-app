# Flipper Wallet App

Co-pilot generated configurable wallet for Flipper Zero with typed fields (String, Number, Date) and persistent storage.

![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-F7-blue) ![License: MIT](https://img.shields.io/badge/License-MIT-green) ![C99](https://img.shields.io/badge/Language-C99-orange)

## Features

- **Configurable Categories**: Vehicles, IDs, or custom via `wallet.conf`
- **Typed Fields**: String (keyboard), Number (pad), Date (picker)
- **Sequential Editing**: Edit multiple fields without returning to list
- **Quick Edit**: Long-press Right on entry
- **Persistent Storage**: Flipper Format (industry-standard)
- **Proper Navigation**: Back returns to previous screen, long-press exits

## Default Categories

**Vehicles**: Name, Year (Number), License Plate, VIN

**IDs**: Name, ID Number, Valid from (Date), Expiry (Date), Issuer

## Installation

**Download**: Get `.fap` from [Releases](#) → Copy to `/data/apps/Tools/wallet_app.fap` → Launch from Tools menu

**Build from Source** (standalone, via [ufbt](https://pypi.org/project/ufbt/) — no firmware clone needed):
```bash
pip install ufbt
git clone https://github.com/YOUR_USERNAME/flipper-wallet-app.git
cd flipper-wallet-app

# Optional: target the Unleashed SDK instead of the official one
ufbt update --index-url=https://up.unleashedflip.com/directory.json --channel=dev

ufbt              # builds dist/wallet_app.fap
ufbt launch       # builds, installs and runs on a connected Flipper
```

## Usage

**Launch**: Applications → Tools → Wallet

**Add Entry**: Select category → OK → "Add new" → Fill fields sequentially

**Edit Entry**: 
- Method 1: Press OK → Press Center to edit each field
- Method 2: Long-press Right for quick edit

**Controls**:
- Up/Down: Navigate
- OK: Confirm
- Right (hold): Quick edit
- Back: Previous screen
- Back (hold): Exit app

## Configuration

**File**: `/data/wallet/wallet.conf`

**Field Types**:
- `String` - Text (up to 32 chars)
- `Number` - Integer (INT32 range)
- `Date` - YYYY-MM-DD format (2000-2099)

**Format**:
```
Filetype: Flipper Wallet Config
Version: 1
Category: Vehicles
Fields: Name[String],Year[Number],LPlate[String],VIN[String]
Category: Custom
Fields: Field1[String],Field2[Number],Field3[Date]
```

**To Edit**: Connect Flipper to computer, edit `/data/wallet/wallet.conf`, restart app

## Storage

**Location**: `/data/wallet/`
- `wallet.conf` - Configuration
- `[CategoryName].txt` - Entry data (pipe-separated)

**Format**:
```
Filetype: Flipper Wallet Data
Version: 1
Entry: Toyota|2020|AB12CDE|1HGBH41JXMN109186
Entry: Honda|2018|XY99ZZZ|2HGES16561H542891
```

**Limits**: 8 categories max, 6 fields per category, 64 entries per category, 32 chars per field

## Development

**Structure**: `application.fam` (manifest), `wallet_app.c` (~750 lines), `icon.png`

**Key Functions**:
- Config: `wallet_app_load_config()`, `wallet_app_parse_fields()`
- Entries: `wallet_app_load_entries()`, `wallet_app_save_entries()`
- UI: `wallet_app_begin_field_edit()` (dispatches TextInput/NumberInput/DateTimeInput)
- List: `EntryListModel` with scrolling, custom drawing
- Nav: `wallet_app_navigation_callback()` (state machine)

See [DEVELOPMENT.md](DEVELOPMENT.md) for build setup and customization.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| App won't launch | Verify FAP in `/data/apps/Tools/` |
| Can't see entries | Check `/data/wallet/` exists, FAP exited cleanly |
| Config not loading | Verify format exactly matches spec |
| Special chars fail | Avoid pipe `\|` and newlines |

## License

MIT License - see [LICENSE](LICENSE)

## Resources

- [DEVELOPMENT.md](DEVELOPMENT.md) - Build and customization guide
- [CHANGELOG.md](CHANGELOG.md) - Version history
- [Flipper Zero Docs](https://docs.flipperzero.io/)
- [Unleashed Firmware](https://github.com/DarkFlippers/unleashed-firmware)

Built for Flipper Zero with standard Flipper APIs.
