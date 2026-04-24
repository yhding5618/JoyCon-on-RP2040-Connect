import { useEffect, useRef, useState } from "react";

type KeyboardCapture = {
  observedPressedCodes: string[];
  capturedPressedCodes: string[];
  windowFocused: boolean;
  clear: () => void;
};

export function useKeyboardCapture(
  captureEnabled: boolean,
  onToggleCapture: () => void,
): KeyboardCapture {
  const [observedPressedCodes, setObservedPressedCodes] = useState<string[]>([]);
  const [capturedPressedCodes, setCapturedPressedCodes] = useState<string[]>([]);
  const [windowFocused, setWindowFocused] = useState<boolean>(true);
  const captureEnabledRef = useRef(captureEnabled);
  const onToggleCaptureRef = useRef(onToggleCapture);

  useEffect(() => {
    captureEnabledRef.current = captureEnabled;
  }, [captureEnabled]);

  useEffect(() => {
    onToggleCaptureRef.current = onToggleCapture;
  }, [onToggleCapture]);

  useEffect(() => {
    if (!captureEnabled) {
      setObservedPressedCodes([]);
      setCapturedPressedCodes([]);
      return;
    }

    setCapturedPressedCodes(
      observedPressedCodes.filter((code) => code !== "F8"),
    );
  }, [captureEnabled, observedPressedCodes]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.code === "F8") {
        event.preventDefault();
        setObservedPressedCodes((current) => appendCode(current, event.code));
        if (!event.repeat) {
          onToggleCaptureRef.current();
        }
        return;
      }

      if (event.repeat) {
        return;
      }

      if (!captureEnabledRef.current) {
        return;
      }

      event.preventDefault();
      setObservedPressedCodes((current) => appendCode(current, event.code));
      setCapturedPressedCodes((current) => appendCode(current, event.code));
    };

    const handleKeyUp = (event: KeyboardEvent) => {
      if (event.code === "F8") {
        event.preventDefault();
      }

      setObservedPressedCodes((current) =>
        current.filter((code) => code !== event.code),
      );
      setCapturedPressedCodes((current) =>
        current.filter((code) => code !== event.code),
      );
    };

    const handleBlur = () => {
      setWindowFocused(false);
      setObservedPressedCodes([]);
      setCapturedPressedCodes([]);
    };

    const handleFocus = () => {
      setWindowFocused(true);
    };

    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    window.addEventListener("blur", handleBlur);
    window.addEventListener("focus", handleFocus);

    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("keyup", handleKeyUp);
      window.removeEventListener("blur", handleBlur);
      window.removeEventListener("focus", handleFocus);
    };
  }, []);

  return {
    observedPressedCodes,
    capturedPressedCodes,
    windowFocused,
    clear: () => {
      setObservedPressedCodes([]);
      setCapturedPressedCodes([]);
    },
  };
}

function appendCode(current: string[], code: string): string[] {
  return current.includes(code) ? current : [...current, code];
}
