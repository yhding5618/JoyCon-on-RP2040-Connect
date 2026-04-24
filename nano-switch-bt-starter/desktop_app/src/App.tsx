import { CapturePanel } from "./components/CapturePanel";
import { ControllerPreview } from "./components/ControllerPreview";
import { DiagnosticsPanel } from "./components/DiagnosticsPanel";
import { MappingPanel } from "./components/MappingPanel";
import { SerialPanel } from "./components/SerialPanel";
import { useControllerInput } from "./hooks/useControllerInput";

export default function App() {
  const {
    appState,
    latestSnapshot,
    isLoading,
    error,
    captureEnabled,
    setCaptureEnabled,
    requestPointerLock,
    releasePointerLock,
    releaseAll,
    surfaceRef,
    pendingAction,
    selectSerialPort,
    refreshPorts,
    connectSerial,
    disconnectSerial,
    requestStatus,
    requestEvents,
    requestVirtualCableUnplug,
    requestClearBonds,
    tapLeftJoyConButton,
  } = useControllerInput();

  const activeBindingCount = Object.keys(appState.profile.activeProfile.bindings).length;
  const isBusy = isLoading || pendingAction !== null;

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
          <span className="hero-metric">{activeBindingCount} bindings</span>
          <span className="hero-metric">
            {appState.serial.connectionState}
          </span>
        </div>
      </header>

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
            onGetEvents={requestEvents}
            onVirtualCableUnplug={requestVirtualCableUnplug}
            onClearBonds={requestClearBonds}
            onTapButton={tapLeftJoyConButton}
          />
          <CapturePanel
            input={appState.input}
            latestSnapshot={latestSnapshot}
            captureEnabled={captureEnabled}
            onSetCaptureEnabled={setCaptureEnabled}
            onRequestPointerLock={requestPointerLock}
            onReleasePointerLock={releasePointerLock}
            onReleaseAll={releaseAll}
            surfaceRef={surfaceRef}
          />
        </div>

        <div className="column-stack">
          <MappingPanel
            profile={appState.profile.activeProfile}
            controllerModel={appState.profile.activeProfile.controllerModel}
          />
          <ControllerPreview controller={appState.controller} />
          <DiagnosticsPanel diagnostics={appState.diagnostics} error={error} />
        </div>
      </section>
    </main>
  );
}
