import assert from 'node:assert/strict';
import fs from 'node:fs';

const d = JSON.parse(fs.readFileSync(new URL('../diagram.json', import.meta.url), 'utf8'));
const parts = new Map(d.parts.map(p => [p.id, p]));
assert.equal(parts.size, d.parts.length, 'Unique part IDs');
for (const [a, b] of d.connections) {
  for (const pin of [a, b]) {
    assert(parts.has(pin.split(':')[0]) || pin.startsWith('$serialMonitor:'), `Unknown part: ${pin}`);
  }
}
function wire(a, b) {
  assert(d.connections.some(([x, y]) => (x === a && y === b) || (x === b && y === a)), `${a} -> ${b}`);
}
assert.equal(parts.get('esp').type, 'board-esp32-devkit-c-v4');
for (const [a, b] of [
  ['esp:18', 'servo1:PWM'], ['esp:19', 'servo2:PWM'],
  ['esp:23', 'logic1:D0'], ['esp:26', 'logic1:D1'], ['esp:GND.1', 'logic1:GND'],
  ['esp:27', 'btn1:1.l'], ['esp:GND.1', 'btn1:2.r'],
  ['esp:34', 'pot1:SIG'], ['esp:3V3', 'pot1:VCC'], ['esp:GND.1', 'pot1:GND'],
]) wire(a, b);
for (const [gpio, led, resistor, color] of [
  [25, 'led3', 'r2', 'green'], [32, 'led1', 'r1', 'red'], [33, 'led2', 'r3', 'yellow'],
]) {
  wire(`esp:${gpio}`, `${resistor}:2`);
  wire(`${resistor}:1`, `${led}:A`);
  wire(`${led}:C`, 'esp:GND.1');
  assert.equal(parts.get(led).attrs.color, color);
  const ohms = Number(parts.get(resistor).attrs.value);
  assert(ohms >= 220 && ohms <= 330);
}
assert(!d.connections.some(([a, b]) => [a, b].includes('pot1:VCC') && [a, b].includes('esp:5V')));
console.log('PASS diagram: fixed GPIO map, E-Stop, 3V3 potentiometer, LEDs/resistors and analyzer');
