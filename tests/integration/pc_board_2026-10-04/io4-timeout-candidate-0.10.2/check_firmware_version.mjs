import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {validateUf2} from "file:///C:/Users/admin/Documents/FluxRail/web/src/update/uf2.js";
const header=await readFile("C:\\Users\\admin\\Documents\\FluxRail\\firmware\\FluxRail\\src\\common\\FirmwareInfo.h",'utf8');
const expected=/^#define FLUXRAIL_FIRMWARE_VERSION "([^"]+)"/m.exec(header)?.[1];
const checked=validateUf2(new Uint8Array(await readFile("C:\\Users\\admin\\Documents\\FluxRail\\tests\\integration\\pc_board_2026-10-04\\io4-timeout-candidate-0.10.2\\build\\arduino\\FluxRail.ino.uf2")));
assert.equal(checked.firmware.version,expected);assert.equal(expected,'0.10.2');console.log(JSON.stringify({...checked,verified:true}));
