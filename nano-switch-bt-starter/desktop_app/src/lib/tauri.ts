import { invoke } from "@tauri-apps/api/core";

type TauriCommandArgs = Record<string, unknown> | undefined;

export function isTauriRuntime(): boolean {
  return "__TAURI_INTERNALS__" in window;
}

export async function invokeTauri<T>(
  command: string,
  args?: TauriCommandArgs,
  fallback?: () => Promise<T> | T,
): Promise<T> {
  if (!isTauriRuntime()) {
    if (!fallback) {
      throw new Error(`Tauri runtime unavailable for command: ${command}`);
    }

    return Promise.resolve(fallback());
  }

  return invoke<T>(command, args);
}
