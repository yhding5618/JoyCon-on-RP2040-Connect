import { useEffect, useRef, useState } from "react";
import type { MutableRefObject } from "react";

type MouseCapture = {
  surfaceRef: MutableRefObject<HTMLDivElement | null>;
  pointerLocked: boolean;
  mouseDeltaX: number;
  mouseDeltaY: number;
  requestPointerLock: () => void;
  releasePointerLock: () => void;
  resetMouseDelta: () => void;
};

export function useMouseCapture(
  enabled: boolean,
  movementEnabled: boolean,
): MouseCapture {
  const surfaceRef = useRef<HTMLDivElement | null>(null);
  const [pointerLocked, setPointerLocked] = useState(false);
  const [mouseDeltaX, setMouseDeltaX] = useState(0);
  const [mouseDeltaY, setMouseDeltaY] = useState(0);
  const movementEnabledRef = useRef(movementEnabled);

  useEffect(() => {
    movementEnabledRef.current = movementEnabled;

    if (!movementEnabled) {
      setMouseDeltaX(0);
      setMouseDeltaY(0);
    }
  }, [movementEnabled]);

  useEffect(() => {
    const handlePointerLockChange = () => {
      const active = document.pointerLockElement === surfaceRef.current;
      setPointerLocked(active);

      if (!active) {
        setMouseDeltaX(0);
        setMouseDeltaY(0);
      }
    };

    const handleMouseMove = (event: MouseEvent) => {
      if (!enabled || document.pointerLockElement !== surfaceRef.current) {
        return;
      }

      if (!movementEnabledRef.current) {
        return;
      }

      setMouseDeltaX((value) => value + event.movementX);
      setMouseDeltaY((value) => value + event.movementY);
    };

    const handleKeyUp = (event: KeyboardEvent) => {
      if (event.code === "AltLeft") {
        setMouseDeltaX(0);
        setMouseDeltaY(0);
      }
    };

    document.addEventListener("pointerlockchange", handlePointerLockChange);
    document.addEventListener("mousemove", handleMouseMove);
    document.addEventListener("keyup", handleKeyUp);

    return () => {
      document.removeEventListener(
        "pointerlockchange",
        handlePointerLockChange,
      );
      document.removeEventListener("mousemove", handleMouseMove);
      document.removeEventListener("keyup", handleKeyUp);
    };
  }, [enabled]);

  useEffect(() => {
    if (!enabled) {
      document.exitPointerLock();
      setMouseDeltaX(0);
      setMouseDeltaY(0);
    }
  }, [enabled]);

  return {
    surfaceRef,
    pointerLocked,
    mouseDeltaX,
    mouseDeltaY,
    requestPointerLock: () => {
      surfaceRef.current?.requestPointerLock();
    },
    releasePointerLock: () => {
      document.exitPointerLock();
    },
    resetMouseDelta: () => {
      setMouseDeltaX(0);
      setMouseDeltaY(0);
    },
  };
}
