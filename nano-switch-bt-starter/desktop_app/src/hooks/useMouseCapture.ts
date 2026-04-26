import { useEffect, useRef, useState } from "react";
import type { MutableRefObject } from "react";

type MouseCapture = {
  surfaceRef: MutableRefObject<HTMLDivElement | null>;
  pointerLocked: boolean;
  altLeftHeld: boolean;
  mouseDeltaX: number;
  mouseDeltaY: number;
  requestPointerLock: () => void;
  releasePointerLock: () => void;
  resetMouseDelta: () => void;
};

export function useMouseCapture(enabled: boolean): MouseCapture {
  const surfaceRef = useRef<HTMLDivElement | null>(null);
  const [pointerLocked, setPointerLocked] = useState(false);
  const [altLeftHeld, setAltLeftHeld] = useState(false);
  const [mouseDeltaX, setMouseDeltaX] = useState(0);
  const [mouseDeltaY, setMouseDeltaY] = useState(0);
  const altLeftHeldRef = useRef(false);

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
      if (!enabled || !altLeftHeldRef.current) {
        return;
      }

      if (
        document.pointerLockElement !== null &&
        document.pointerLockElement !== surfaceRef.current
      ) {
        return;
      }

      setMouseDeltaX((value) => value + event.movementX);
      setMouseDeltaY((value) => value + event.movementY);
    };

    const handleKeyDown = (event: KeyboardEvent) => {
      if (!enabled || event.code !== "AltLeft") {
        return;
      }

      event.preventDefault();
      altLeftHeldRef.current = true;
      setAltLeftHeld(true);
    };

    const handleKeyUp = (event: KeyboardEvent) => {
      if (event.code === "AltLeft") {
        event.preventDefault();
        altLeftHeldRef.current = false;
        setAltLeftHeld(false);
        setMouseDeltaX(0);
        setMouseDeltaY(0);
      }
    };

    const handleBlur = () => {
      altLeftHeldRef.current = false;
      setAltLeftHeld(false);
      setMouseDeltaX(0);
      setMouseDeltaY(0);
    };

    document.addEventListener("pointerlockchange", handlePointerLockChange);
    document.addEventListener("mousemove", handleMouseMove);
    document.addEventListener("keydown", handleKeyDown, true);
    document.addEventListener("keyup", handleKeyUp, true);
    window.addEventListener("blur", handleBlur);

    return () => {
      document.removeEventListener(
        "pointerlockchange",
        handlePointerLockChange,
      );
      document.removeEventListener("mousemove", handleMouseMove);
      document.removeEventListener("keydown", handleKeyDown, true);
      document.removeEventListener("keyup", handleKeyUp, true);
      window.removeEventListener("blur", handleBlur);
    };
  }, [enabled]);

  useEffect(() => {
    if (!enabled) {
      altLeftHeldRef.current = false;
      setAltLeftHeld(false);
      document.exitPointerLock();
      setMouseDeltaX(0);
      setMouseDeltaY(0);
    }
  }, [enabled]);

  return {
    surfaceRef,
    pointerLocked,
    altLeftHeld,
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
