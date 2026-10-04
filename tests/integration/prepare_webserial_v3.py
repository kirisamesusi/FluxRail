from pathlib import Path
p=Path(__file__).with_name('test_webserial_offscreen_v2.cjs')
o=p.with_name('test_webserial_offscreen_v3.cjs')
assert not o.exists()
s=p.read_text(encoding='utf-8').replace("'webserial-offscreen-v2'", "'webserial-offscreen-v3'")
s=s.replace("const origin='http://localhost:4174';", """const origin='http://localhost:4174';
const correlation=JSON.parse(fs.readFileSync(path.join(root,'webserial-pnp-correlation.json'),'utf8').replace(/^\\uFEFF/,''));
assert.equal(correlation.parent,'USB\\\\VID_239A&PID_CAFE\\\\5054316560DD729C');
assert.equal(correlation.port,'COM29');
assert(Date.now()-Date.parse(correlation.time)<300000,'Fresh PnP identity required');""")
s=s.replace("p.serialNumber==='5054316560DD729C'&&p.portName==='COM29'", "p.portName===correlation.port&&p.deviceInstanceId===correlation.deviceInstanceId&&Number(p.vendorId)===0x239a&&Number(p.productId)===0xcafe")
s=s.replace("ports:[],error:null", "ports:[],pnp_correlation:correlation,error:null")
o.write_text(s,encoding='utf-8')
