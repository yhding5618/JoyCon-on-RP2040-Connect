import { AutomationPanel } from "./components/AutomationPanel";
import { CommandLogPanel } from "./components/CommandLogPanel";
import { ControllerPreview } from "./components/ControllerPreview";
import { Panel } from "./components/Panel";
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
    clearCommandLog,
    turnOnBluetoothWithMode,
    setBluetoothEnabled,
    tapControllerButton,
    startAutomation,
    stopAutomation,
  } = useControllerInput();

  const isBusy = isLoading || pendingAction !== null;
  const automationRunning = appState.automation.running;
  const visiblePressedCodes = captureEnabled ? keyboard.observedPressedCodes : [];

  return (
    <main className="app-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Rust + Tauri MVP</p>
          <h1>Nano Switch Desktop</h1>
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
        <div className="column-stack utility-column">
          <SerialPanel
            serial={appState.serial}
            loading={isBusy}
            onRefreshPorts={refreshPorts}
            onSelectPort={selectSerialPort}
            onConnect={connectSerial}
            onDisconnect={disconnectSerial}
          />
          <CommandLogPanel
            entries={appState.diagnostics.commandLog}
            onClear={clearCommandLog}
          />
        </div>

        <div className="column-stack main-workbench">
          <ControllerPreview
            profile={appState.profile.activeProfile}
            serial={appState.serial}
            controller={appState.controller}
            latestSnapshot={latestSnapshot}
            automationRunning={automationRunning}
            captureEnabled={captureEnabled}
            loading={isBusy}
            observedPressedCodes={visiblePressedCodes}
            directTapOverlay={directTapOverlay}
            directTapAvailable={
              appState.serial.connectionState === "Connected" &&
              !isBusy &&
              !automationRunning
            }
            onTurnOnBluetoothWithMode={turnOnBluetoothWithMode}
            onSetBluetoothEnabled={setBluetoothEnabled}
            onTapControllerButton={tapControllerButton}
          />
        </div>

        <div className="column-stack automation-column">
          <Panel title="Automation" className="automation-panel-shell">
            <AutomationPanel
              automation={appState.automation}
              loading={isBusy}
              onStart={startAutomation}
              onStop={stopAutomation}
            />
          </Panel>
        </div>
      </section>
    </main>
  );
}
