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

export function useMouseCapture(enabled: boolean): MouseCapture {
  const surfaceRef = useRef<HTMLDivElement | null>(null);
  const [pointerLocked, setPointerLocked] = useState(false);
  const [mouseDeltaX, setMouseDeltaX] = useState(0);
  const [mouseDeltaY, setMouseDeltaY] = useState(0);

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

      setMouseDeltaX((value) => value + event.movementX);
      setMouseDeltaY((value) => value + event.movementY);
    };

    document.addEventListener("pointerlockchange", handlePointerLockChange);
    document.addEventListener("mousemove", handleMouseMove);

    return () => {
      document.removeEventListener(
        "pointerlockchange",
        handlePointerLockChange,
      );
      document.removeEventListener("mousemove", handleMouseMove);
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
