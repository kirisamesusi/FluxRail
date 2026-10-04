import { assert, verifyHello, validateConfig, validateProfile, validName } from './config.js';
/** 页面状态只在一致读回后发布。保留草稿基准revision，轮询得到新revision也不能
 * 将旧草稿伪装为新版本写回；冲突必须由用户读回处理。所有变化走现有公共命令。
 */
export class Device {
  constructor(client) { this.client=client; this.selected=null; this.ready=false; }
  async handshake() { this.hello=verifyHello(await this.client.request('hello')); await this.refresh(); }
  async refresh(selected=this.selected) {
    const status=await this.client.request('get_status');const revision=this.client.revision;
    assert(status.config_ready && !status.applying && status.maintenance==='idle','设备仍在应用/维护，请稍后读回');
    let meta=null, profiles=[], profile;
    const check=()=>assert(this.client.revision===revision,'读取期间配置变化，请重新读回');
    if(this.hello.capabilities.profiles) {
      meta=await this.client.request('get_device');check();assert([1,2].includes(meta.schema),'设备schema不支持');
      const list=await this.client.request('list_profiles');check();profiles=list.profiles;
      assert(Array.isArray(profiles)&&profiles.length>=4&&profiles.length<=12&&list.max_profiles===12,'模板列表无效');
      selected=profiles.some(p=>p.profile_id===selected)?selected:meta.active_profile;
      profile=await this.client.request('get_profile',{profile_id:selected});check();validateProfile(profile);
    } else {
      const result=await this.client.request('get_config');check();assert(result.config_ready,'模块1尚未应用');validateConfig(result.config);
      profile={profile_id:null,name:'运行配置',config:result.config,motion:null};
    }
    this.status=status;this.meta=meta;this.profiles=profiles;this.profile=profile;
    this.selected=selected;this.revision=revision;this.envelope=this.client.last;
    this.draft=structuredClone(profile);this.localDirty=false;this.ready=true;
  }
  async poll() { const status=await this.client.request('get_status');this.status=status;this.latestEnvelope=this.client.last;return status; }
  checkWritable() {
    assert(this.ready && this.client.alive,'请先完成连接读回');
    assert(this.revision===this.client.revision,'设备版本已变化，请读回后重新编辑');
    assert(this.status.config_ready&&!this.status.applying&&this.status.maintenance==='idle','设备正忙，请稍后读回');
  }
  async apply() {
    this.checkWritable(); validateConfig(this.draft.config);
    if(this.hello.capabilities.profiles)validateProfile(this.draft);
    try {
      if(this.hello.capabilities.profiles)await this.client.request('set_profile',{expected_revision:this.revision,profile_id:this.selected,config:this.draft.config,motion:this.draft.motion},4000);
      else await this.client.request('set_config',{expected_revision:this.revision,config:this.draft.config},4000);
      await this.refresh();
    } catch(error) { this.ready=false; throw error; } // 禁止结果未知时再次写入，先完整读回。
  }
  async mutate(op,extra={}) {
    this.checkWritable();assert(!this.localDirty,'请先应用或读回舍弃网页草稿');
    if(extra.name!==undefined)validName(extra.name);
    try {
      await this.client.request(op,{expected_revision:this.revision,...extra},op==='save_config'?12000:4000);
      await this.refresh();
    } catch(error) { this.ready=false; throw error; }
  }
}
