import { ControllerPreview } from "./components/ControllerPreview";
import { SerialPanel } from "./components/SerialPanel";
import { useControllerInput } from "./hooks/useControllerInput";

export default function App() {
  const {
    appState,
    latestSnapshot,
    isLoading,
    error,
    captureEnabled,
    pendingAction,
    keyboard,
    directTapOverlay,
    selectSerialPort,
    refreshPorts,
    connectSerial,
    disconnectSerial,
    requestStatus,
    requestVirtualCableUnplug,
    requestClearBonds,
    setControllerMode,
    setBluetoothEnabled,
    tapControllerButton,
  } = useControllerInput();

  const isBusy = isLoading || pendingAction !== null;
  const visiblePressedCodes = captureEnabled ? keyboard.observedPressedCodes : [];

  return (
    <main className="app-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Rust + Tauri MVP</p>
          <h1>Nano Switch Desktop</h1>
          <p className="hero-copy">
            Focused-window capture, Rust-owned mapping, and the existing
            RP2040 bridge protocol.
          </p>
        </div>
        <div className="hero-status">
          <span
            className={`status-badge ${
              captureEnabled ? "status-badge-active" : ""
            }`}
          >
            {captureEnabled ? "Capture Active" : "Capture Disabled"}
          </span>
          <span className="hero-metric">
            {appState.serial.connectionState}
          </span>
        </div>
      </header>

      {error ? <div className="app-error">{error}</div> : null}

      <section className="layout-grid">
        <div className="column-stack">
          <SerialPanel
            serial={appState.serial}
            controllerModel={appState.profile.activeProfile.controllerModel}
            loading={isBusy}
            onRefreshPorts={refreshPorts}
            onSelectPort={selectSerialPort}
            onConnect={connectSerial}
            onDisconnect={disconnectSerial}
            onGetStatus={requestStatus}
            onVirtualCableUnplug={requestVirtualCableUnplug}
            onClearBonds={requestClearBonds}
            onSetControllerMode={setControllerMode}
            onSetBluetoothEnabled={setBluetoothEnabled}
          />
        </div>

        <div className="column-stack">
          <ControllerPreview
            profile={appState.profile.activeProfile}
            controller={appState.controller}
            latestSnapshot={latestSnapshot}
            captureEnabled={captureEnabled}
            observedPressedCodes={visiblePressedCodes}
            directTapOverlay={directTapOverlay}
            directTapAvailable={
              appState.serial.connectionState === "Connected" && !isBusy
            }
            onTapControllerButton={tapControllerButton}
          />
        </div>
      </section>
    </main>
  );
}
