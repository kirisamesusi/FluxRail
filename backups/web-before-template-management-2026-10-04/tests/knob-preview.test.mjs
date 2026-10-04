import test from 'node:test';
import assert from 'node:assert/strict';
import { angleDelta, boundedTurns, projectPreview } from '../src/knob-preview.js';

test('本地预览跨0°拖动保持连续方向，不将359→1误判成倒转',()=>{
  assert.equal(angleDelta(359,1),2);assert.equal(angleDelta(1,359),-2);
  assert.equal(angleDelta(179,-179),2);assert.equal(angleDelta(-179,179),-2);
});
test('倍率1/2/0.5保留累计圈数；完整一圈方向相同但圈数仍可区分',()=>{
  const config={gain:1,invert:false,axis:0};
  assert.equal(projectPreview(1,config).scaledTurns,1);
  assert.equal(projectPreview(1,config).scaledAngle,0);
  assert.equal(projectPreview(1,{...config,gain:2}).scaledTurns,2);
  assert.equal(projectPreview(1,{...config,gain:.5}).scaledAngle,180);
});
test('反向和负圈位置归一化；只投影，不改共享配置',()=>{
  const config=Object.freeze({gain:2,invert:true,axis:1});
  const p=projectPreview(.25,config);assert.equal(p.scaledTurns,-.5);assert.equal(p.scaledAngle,180);assert.equal(p.rawAngle,90);assert.equal(p.axis,'Y');
  assert.equal(projectPreview(-.25,config).rawAngle,270);
});
test('本地连续模拟圈数有界，避免无限累积渲染角度',()=>{
  assert.equal(boundedTurns(1001),1000);assert.equal(boundedTurns(-1001),-1000);assert.equal(boundedTurns(.125),.125);
});
