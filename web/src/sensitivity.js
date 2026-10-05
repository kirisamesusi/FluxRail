/** Product sensitivity scale. CDC/schema2 continue storing the actual gain.
 * 1.0..10.0 in 0.1 steps; 5.0 preserves the original gain 1.
 * Never quantize a gain merely because it was read from a device/archive.
 */
export const SENSITIVITY_MIN = 1;
export const SENSITIVITY_MAX = 10;
export function normalizeSensitivity(value) {
  if (!Number.isFinite(value) || value < SENSITIVITY_MIN || value > SENSITIVITY_MAX)
    throw new RangeError('灵敏度应为 1.0～10.0');
  return Math.round((value + Number.EPSILON) * 10) / 10;
}
export function sensitivityToGain(value) {
  return Math.round(normalizeSensitivity(value) * 10) / 50;
}
export function gainView(gain, precision = 'f64') {
  const scale = gain * 5;
  const rounded = Math.round(scale * 10) / 10;
  // Compare against the actual storage representation, rather than a broad
  // quantization tolerance that could conceal a distinct old knob setting.
  const canonical = Math.round(rounded * 10) / 50;
  const sameGain = precision === 'f32' ? Math.fround(gain) === Math.fround(canonical)
    : Math.abs(gain - canonical) <= Number.EPSILON * Math.max(1, Math.abs(canonical)) * 4;
  const representable = Number.isFinite(scale) && rounded >= 1 && rounded <= 10 && sameGain;
  return { value: representable ? rounded : null, legacy: !representable };
}
export function sensitivityText(gain) {
  const view = gainView(gain);
  return view.legacy ? '旧配置（原倍率 ' + gain + '，保留）' : view.value.toFixed(1);
}
