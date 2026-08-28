# Changelog

All notable changes to the Flipper Wallet App will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2024

### Added
- Initial release of Flipper Wallet App
- Multi-category document organization system
- Support for three field types: String, Number, Date
- Typed input widgets for each field type:
  - Text input with on-screen keyboard for strings
  - Number pad for numeric input
  - Date picker with year/month/day selection
- Sequential field editing workflow - edit multiple fields without returning to list
- Quick edit functionality via long-press Right on entry
- Custom entry list view with visual scrolling
- Persistent storage using Flipper Format (industry-standard)
- Configurable document categories via `wallet.conf`
- Default categories: Vehicles and IDs
- Proper navigation with Back button returning to previous screen
- Long-press Back to exit from any screen
- Maximum 8 categories, 6 fields per category, 64 entries per category
- Field values up to 32 characters
- Date support with YYYY-MM-DD format (2000-2099 range)

### Technical Details
- ~750 lines of C99 code
- Uses Flipper's standard configuration format (Flipper Format)
- Implements ViewDispatcher state machine for navigation
- Custom entry list view model with windowed rendering
- Safe string operations throughout
- Efficient memory usage (~15 KB)
- Compatible with Flipper Zero F7 hardware

### Building
- Requires Flipper firmware development environment
- Build command: `fbt fap_wallet_app`
- Produces FAP (Flipper Application Package) artifact

### Dependencies
- Flipper GUI module (gui)
- Flipper Storage module (storage)
- Flipper Format library (flipper_format)
- Standard datetime utilities

## Future Enhancements (Planned)
- [ ] Search functionality
- [ ] Export/import entries to JSON or CSV
- [ ] Additional field types (Phone, Email, URL)
- [ ] Entry templates for quick creation
- [ ] Backup/restore with encryption
- [ ] Category icons
- [ ] Field validation (regex patterns)
- [ ] Multi-line text fields
- [ ] Photo attachment support
