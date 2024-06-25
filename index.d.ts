/// <reference types="node">

/** allocates and writes buffer to the shared memory at name */
declare export function shmWrite(name: string, buffer: Buffer): void;
/** releases the shared memory at name */
declare export function shmUnlink(name: string): void;
/** sets termios attributes to allow realtime updates for key input */
declare export function setupTermios(): void;
/** restores termios attributes to the original attributes before calling setupTermios */
declare export function cleanupTermios(): void;
