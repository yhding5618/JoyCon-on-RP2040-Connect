# Desktop App

Tauri v2 desktop app for driving the existing RP2040 bridge from a focused-window React UI.

## Scope

This app follows the staged plan in `RUST_TAURI_DESKTOP_APP_PLAN.md`:

- Tauri v2 shell
- React + TypeScript + Vite frontend
- Rust-owned bridge protocol, mapping, and serial transport
- Focused-window keyboard and pointer-lock mouse capture

The current implementation includes:

- the app scaffold under `desktop_app/`
- a minimal frontend shell with serial, capture, mapping, preview, and diagnostics panels
- Rust-side app state and input snapshot commands
- the bridge protocol module and tests
- a default Left Joy-Con profile and preview mapping

Serial transport is scaffolded but not connected yet. That comes next in Phase 2.

## Run

1. Install frontend dependencies:

```powershell
npm install
```

2. Run the frontend in a browser:

```powershell
npm run dev
```

3. Run the desktop app once Tauri dependencies are installed:

```powershell
npm run tauri dev
```

## Validate

Rust protocol tests:

```powershell
cargo test
```

Frontend type-check and bundle:

```powershell
npm run build
```
