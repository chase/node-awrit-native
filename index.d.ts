/// <reference types="node">

/** allocates and writes buffer to the shared memory at name */
export declare function shmWrite(name: string, buffer: Buffer): void;
/** releases the shared memory at name */
export declare function shmUnlink(name: string): void;
/** sets termios attributes to allow realtime updates for key input */
export declare function setupInput(): void;
/** restores termios attributes to the original attributes before calling setupTermios */
export declare function cleanupInput(): void;

export declare enum KeyEvent {
  Invalid = 0,
  Down = 1,
  Repeat = 2,
  Up = 3,
  Unicode = 4,
}

export declare enum MouseEvent {
  Down = 0,
  Up = 1,
  Move = 2,
}

export declare enum EscapeType {
  None = 0,
  CSI = 1,
  OSC = 2,
  DCS = 3,
  PM = 4,
  SOS = 5,
  APC = 6,
  Key = 7,
  Mouse = 8,
}

export declare enum MouseModifier {
  Shift = 1 << 2,
  Alt = 1 << 3,
  Ctrl = 1 << 4,
  Motion = 1 << 5,
  None = 0,
}

export declare enum MouseButton {
  Left = 1 << 0,
  Middle = 1 << 1,
  Right = 1 << 2,
  Fourth = 1 << 3,
  Fifth = 1 << 4,
  Sixth = 1 << 5,
  Seventh = 1 << 6,
  WheelUp = 1 << 7,
  WheelDown = 1 << 8,
  WheelLeft = 1 << 9,
  WheelRight = 1 << 10,
  None = 0,
}

export type InputEvent =
  | {
      type: EscapeType.Key;
      event: KeyEvent;
      code: string;
    }
  | {
      type: EscapeType.Mouse;
      event: MouseEvent;
      buttons: number;
      modfiers: number;
      x?: number;
      y?: number;
    }
  | {
      type:
        | EscapeType.CSI
        | EscapeType.OSC
        | EscapeType.DCS
        | EscapeType.PM
        | EscapeType.SOS
        | EscapeType.APC;
      data: string;
    };

/**
 * Listens for input events at intervalMs, defaults to 10ms
 * @param cb the callback to be called when an event occurs
 * @param intervalMs defaults to 10ms
 * @returns Cancel callback
 */
export declare function listenForInput(
  cb: (evt: InputEvent) => void,
  intervalMs?: number,
): () => void;
