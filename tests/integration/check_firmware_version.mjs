/** Verify the formal artifact using the same parser as the Web file checker. */
import { readFile } from 'node:fs/promises';
import assert from 'node:assert/strict';
import { validateUf2 } from '../../web/src/update/uf2.js';
const root = new URL('../../', import.meta.url);
const header = await readFile(new URL('firmware/FluxRail/src/common/FirmwareInfo.h', root), 'utf8');
const expected = /^#define FLUXRAIL_FIRMWARE_VERSION "([^"]+)"/m.exec(header)?.[1];
assert.ok(expected, 'Missing release version');
const bytes = new Uint8Array(await readFile(new URL('tests/integration/build/arduino/FluxRail.ino.uf2', root)));
const checked = validateUf2(bytes);
assert.equal(checked.firmware.version, expected, 'UF2 version differs from firmware source');
console.log(JSON.stringify({ ...checked, verified: true }));
