import { useEffect, useState } from "react";

type KeyboardCapture = {
  pressedCodes: string[];
  windowFocused: boolean;
  clearPressedCodes: () => void;
};

export function useKeyboardCapture(enabled: boolean): KeyboardCapture {
  const [pressedCodes, setPressedCodes] = useState<string[]>([]);
  const [windowFocused, setWindowFocused] = useState<boolean>(true);

  useEffect(() => {
    if (!enabled) {
      setPressedCodes([]);
      return;
    }

    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.repeat) {
        return;
      }

      setPressedCodes((current) =>
        current.includes(event.code) ? current : [...current, event.code],
      );
    };

    const handleKeyUp = (event: KeyboardEvent) => {
      setPressedCodes((current) => current.filter((code) => code !== event.code));
    };

    const handleBlur = () => {
      setWindowFocused(false);
      setPressedCodes([]);
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
  }, [enabled]);

  return {
    pressedCodes,
    windowFocused,
    clearPressedCodes: () => setPressedCodes([]),
  };
}
