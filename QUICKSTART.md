# Quick Start

## Installation

**Users**: Download `.fap` → Copy to `/data/apps/Tools/wallet_app.fap` → Launch from Tools

**Developers**: 
```bash
git clone https://github.com/DarkFlippers/unleashed-firmware.git
cd unleashed-firmware
cp -r /path/to/flipper-wallet-app applications_user/wallet_app
fbt fap_wallet_app
```

## Data Storage

- **Config**: `/data/wallet/wallet.conf`
- **Entries**: `/data/wallet/[CategoryName].txt`

## GitHub Setup

```bash
cd flipper-wallet-app
git init
git add .
git commit -m "Initial commit: Flipper Wallet App v1.0"
git remote add origin https://github.com/YOUR_USERNAME/flipper-wallet-app.git
git push -u origin main
```

## Create Release

1. Go to GitHub repository
2. Click "Releases" → "Create a new release"
3. Tag: `v1.0.0`
4. Upload built `.fap` file from `build/f7-firmware-D/.extapps/wallet_app.fap`

## Repository Structure

```
flipper-wallet-app/
├── README.md          # User guide
├── DEVELOPMENT.md     # Build guide
├── CHANGELOG.md       # Version history
├── LICENSE           # MIT License
├── .gitignore
├── application.fam   # App manifest
├── wallet_app.c      # Source (~750 lines)
└── icon.png          # App icon
```

## Key Features

✓ 8 categories, 6 fields per category, 64 entries max per category
✓ Typed fields: String, Number, Date
✓ Quick edit via long-press Right
✓ Flipper Format storage
✓ Configurable via `wallet.conf`
✓ MIT Licensed, ready for GitHub

See [README.md](README.md) for full documentation.
See [DEVELOPMENT.md](DEVELOPMENT.md) for build and customization details.
