/// <reference types="node">

/** allocates and writes buffer to the shared memory at name */
declare export function shmWrite(name: string, buffer: Buffer): void;
/** releases the shared memory at name */
declare export function shmUnlink(name: string): void;
