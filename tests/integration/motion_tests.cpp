// 复用既有真正App/协议/Flash维护测试夹具，并增加传感器到USB的整合回归。
#include "app_storage_tests.cpp"
#include <Sensors.h>
#include <pico/bootrom.h>
struct MotionOutput:app::MotionSink{
  bool blocked=false;int16_t xs[256]{},ys[256]{};uint32_t n=0,abs_n=0;float last_abs=0;
  bool relative(int16_t x,int16_t y)override{if(blocked)return false;if(n<256){xs[n]=x;ys[n]=y;}++n;return true;}
  bool absolute(float p,uint32_t,uint32_t)override{if(blocked)return false;last_abs=p;++abs_n;return true;}
};
static knobs::Motion motion(uint8_t id,knobs::Axis axis,double counts,uint32_t rev=7,uint32_t epoch=9){
  knobs::Motion m;m.knob_id=id;m.axis=axis;m.delta_counts=counts;m.valid=m.enabled=m.active=true;m.revision=rev;m.epoch=epoch;return m;
}
extern "C" int test_router_backpressure_reversal_fraction_and_dual_axis(){
  app::MotionRouter r;r.configure({},7,9,false);MotionOutput out;out.blocked=true;
  r.knob(motion(0,knobs::Axis::X,70000),100);r.knob(motion(1,knobs::Axis::Y,-90000),100);
  r.knob(motion(0,knobs::Axis::X,-70000),101);r.service(out);
  CHECK(out.n==0&&r.pending(0)==0&&r.pending(1)==-90000);
  out.blocked=false;for(uint8_t i=0;i<8;++i)r.service(out);
  CHECK(out.n==6&&out.xs[0]==32767&&out.ys[0]==-32767&&out.xs[3]==-32767);
  CHECK(r.pending(0)==0&&r.pending(1)==0);
  r.clear();r.knob(motion(0,knobs::Axis::X,0.75),200);r.knob(motion(0,knobs::Axis::X,-1),201);r.service(out);
  CHECK(out.xs[out.n-1]==-1&&r.pending(0)==0.75);
  r.knob(motion(0,knobs::Axis::X,0.25),202);r.service(out);CHECK(out.xs[out.n-1]==1&&r.pending(0)==0);
  return 0;
}
extern "C" int test_router_activity_handoff_zero_idle_and_io4(){
  app::MotionRouter r;common::RoutingSettings s;s.idle_us=0;r.configure(s,7,9,false);
  hall::Estimate h;h.valid=h.active=true;h.output=0.8f;h.revision=7;h.epoch=9;
  r.hall(h,100);r.knob(motion(0,knobs::Axis::X,50),100);CHECK(r.owner(0)==1&&r.pending(0)==0);
  r.knob(motion(1,knobs::Axis::Y,10),100);MotionOutput out;r.service(out);CHECK(out.abs_n==1&&out.last_abs==0.8f&&out.ys[0]==10);
  r.knob(motion(0,knobs::Axis::X,3),101);CHECK(r.owner(0)==2);
  r.hall(h,102);CHECK(r.owner(0)==1&&r.stats().discarded_absolute[0]>=53);
  s.simultaneous_hall_first=false;r.configure(s,7,10,false);h.epoch=10;r.hall(h,200);r.knob(motion(0,knobs::Axis::X,3,7,10),200);CHECK(r.owner(0)==2);
  r.configure(s,7,11,true);h.epoch=11;r.hall(h,300);r.knob(motion(0,knobs::Axis::X,100,7,11),300);CHECK(r.owner(0)==1&&r.pending(0)==0);
  return 0;
}
extern "C" int test_router_queue_full_is_counted_and_faulted(){
  app::MotionRouter r;r.configure({},7,9,false);for(uint32_t i=0;i<64;++i)r.knob(motion(0,knobs::Axis::X,2),i);
  CHECK(!r.canAccept(knobs::Axis::X));r.knob(motion(0,knobs::Axis::X,-4),65);
  CHECK(r.faulted()&&r.stats().queue_full==1&&r.stats().discarded_absolute[0]==4);return 0;
}
extern "C" int test_router_joint_xy_busy_same_direction_and_reversal() {
  struct Joint : MotionOutput {
    uint32_t joint=0;
    bool absoluteWithY(float p,int16_t y,uint32_t,uint32_t) override {
      if(blocked) return false;
      last_abs=p; ++abs_n; ++joint; xs[n]=0; ys[n]=y; ++n; return true;
    }
  } out;
  app::MotionRouter r; r.configure({},7,9,false);
  hall::Estimate h; h.valid=h.active=true; h.output=0.75f; h.revision=7; h.epoch=9;
  r.hall(h,100);
  r.knob(motion(1,knobs::Axis::Y,10),100);
  r.knob(motion(1,knobs::Axis::Y,15.75),101);
  r.knob(motion(1,knobs::Axis::Y,-5),102);
  out.blocked=true; r.service(out);
  CHECK(out.n==0 && r.pending(1)==20.75 && r.stats().submitted[1]==0);
  out.blocked=false; r.service(out);
  CHECK(out.joint==1 && out.abs_n==1 && out.ys[0]==25 && out.last_abs==0.75f);
  CHECK(r.pending(1)==-4.25 && r.stats().submitted[1]==25);
  r.service(out); CHECK(out.ys[1]==-5 && r.pending(1)==0.75);
  r.knob(motion(1,knobs::Axis::Y,0.25),103); r.service(out);
  CHECK(out.ys[2]==1 && r.pending(1)==0 && !r.faulted());
  return 0;
}
extern "C" int test_app_real_sampling_relative_push_save_and_resume(){
  MemoryFlash flash;app_flash=&flash;CHECK(usb::normalHid().configureBeforeConnect());
  mocksensor::enabled=true;app::App h;h.beginCore0();h.beginCore1();tickApp(h);CHECK(h.ready());
  auto c=*h.deviceConfig();auto *p=config::find(c,c.active_id);p->knobs[0].enabled=p->knobs[1].enabled=true;p->knobs[1].axis=knobs::Axis::Y;
  p->knobs[0].push.enabled=true;p->runtime.keyboard.bindings[10]={common::ActionType::Keyboard,4,0,0};c.revision=2;
  CHECK(h.applyDevice(c,mockapp::now));tickApp(h);CHECK(h.ready());
  mocksensor::advance_ticks[0]=32;mocksensor::advance_ticks[1]=64;tickApp(h,150);
  CHECK(mocksensor::ssi_reads[0]==mocksensor::ssi_reads[1]&&mocksensor::adc_reads>0);
  CHECK(h.routingStats().submitted[0]>0&&h.routingStats().submitted[1]>0);
  CHECK(h.motionSnapshot()->revision==2&&h.motionSnapshot()->knobs[0].scaled_turns>0&&h.motionSnapshot()->knobs[1].scaled_turns>0);
  mocksensor::advance_ticks[0]=0;mocksensor::advance_ticks[1]=0;tickApp(h,30);mocksensor::push[0]=true;tickApp(h,20);CHECK(h.physicalMask()&(1u<<10));
  CHECK(h.save(mockapp::now));tickApp(h,60);CHECK(!h.saving()&&h.saveResult()==config::SaveResult::Ok&&!(h.physicalMask()&(1u<<10)));
  tickApp(h,20);CHECK(!(h.physicalMask()&(1u<<10)));mocksensor::push[0]=false;tickApp(h,20);mocksensor::push[0]=true;tickApp(h,20);CHECK(h.physicalMask()&(1u<<10));
  return 0;
}
extern "C" int test_app_calibration_start_cancel_other_sources_continue(){
  MemoryFlash flash;app_flash=&flash;CHECK(usb::normalHid().configureBeforeConnect());mocksensor::enabled=true;
  app::App h;h.beginCore0();h.beginCore1();tickApp(h,150);CHECK(h.ready());
  common::CalibrationCommand c;c.op=common::CalibrationOp::Start;c.token=11;c.job_id=101;c.magnet_removed=true;c.left_mm=-41;c.right_mm=39;
  CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,140);CHECK(h.calibrationSucceeded()&&h.calibrating()&&!h.calibrationPending());
  CHECK(h.motionSnapshot()->calibration.stage==hall::CalibrationStage::Baseline);
  auto config=*h.deviceConfig();config.revision=2;CHECK(!h.applyDevice(config,mockapp::now)&&!h.save(mockapp::now));
  const uint32_t reads=mocksensor::ssi_reads[1];mockapp::raw=1;tickApp(h,20);CHECK(h.physicalMask()==1&&mocksensor::ssi_reads[1]>reads);
  c.op=common::CalibrationOp::Cancel;c.token=12;CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,140);
  CHECK(!h.calibrating()&&!h.calibrationPending()&&h.calibrationSucceeded()&&h.deviceConfig()->revision==1&&!h.deviceConfig()->calibration.valid);return 0;
}
extern "C" int test_absolute_hid_encoding_report_ids_and_button_owner(){
  auto &u=usb::normalHid();CHECK(u.configureBeforeConnect());CHECK(u.tryMouseButtons(5));TinyUSB_Device_Task();
  CHECK(u.setAbsoluteMode(true));
  CHECK(u.tryAbsolute(0.75f));const auto &e=mockusb::endpoints[1];CHECK(e.length==6&&e.payload[0]==2&&e.payload[1]==0&&e.payload[4]==0&&e.payload[5]==64);TinyUSB_Device_Task();
  CHECK(u.setAbsoluteMode(false));
  CHECK(u.tryRelative(-12,31));CHECK(e.length==6&&e.payload[0]==1&&e.payload[1]==5&&e.payload[2]==244&&e.payload[4]==31);
  uint8_t bytes[5];CHECK(usb::encodeAbsoluteXY(0,0,bytes)&&bytes[0]==0&&bytes[1]==0&&bytes[2]==0&&bytes[3]==0&&bytes[4]==0);
  CHECK(usb::encodeAbsoluteXY(1,32767,bytes)&&bytes[1]==255&&bytes[2]==127&&bytes[3]==255&&bytes[4]==127);
  CHECK(!usb::encodeAbsoluteXY(-1,0,bytes)&&!usb::encodeAbsoluteXY(2,0,bytes)&&!usb::encodeAbsoluteXY(0,32768,bytes));return 0;
}
static hall::Calibration fixture() {
  hall::Calibration c{};
  c.valid = true;
  c.left_mm = -41;
  c.right_mm = 39;
  c.node_count = 7;
  const float positions[7] = {-41, hall::kSensorMm[0],        hall::kSensorMm[0] * 0.5f,
                              0,   hall::kSensorMm[2] * 0.5f, hall::kSensorMm[2],
                              39};
  c.polarity[1] = -1;
  for (uint8_t i = 0; i < 3; ++i) {
    c.baseline[i] = 2048;
    c.baseline_noise[i] = 0.3f;
  }
  for (uint8_t j = 0; j < 7; ++j) {
    c.nodes[j].mm = positions[j];
    for (uint8_t i = 0; i < 3; ++i) {
      c.nodes[j].response[i] =
          100 + 900 * hall::clamp(1 - hall::magnitude(positions[j] - hall::kSensorMm[i]) / 33.75f,
                            0, 1);
      c.nodes[j].noise[i] = 0.5f;
    }
  }
  c.noise_mm = 0.02f;
  c.sweep_left = true;
  c.sweep_right = true;
  c.sweep_segments = 63;
  return c;
}
extern "C" int test_app_sensor_cadence_independent_of_key_scan_jitter() {
  MemoryFlash flash; app_flash = &flash;
  CHECK(usb::normalHid().configureBeforeConnect());
  mocksensor::enabled = true;
  app::App h; h.beginCore0(); h.beginCore1(); tickApp(h,150);
  const uint32_t start = mockapp::now;
  const uint32_t adc_start = mocksensor::adc_reads;
  const uint32_t ssi_start = mocksensor::ssi_reads[0];
  // 每97us进入loop1；旧500us早退与重新计时会使Hall低于1kHz。
  for (uint32_t dt = 97; dt <= 1000000; dt += 97) {
    mockapp::now = start + dt;
    h.serviceCore1(); h.serviceCore0();
  }
  const uint32_t vectors = (mocksensor::adc_reads-adc_start)/3;
  CHECK(vectors >= 999 && vectors <= 1001);
  CHECK(mocksensor::ssi_reads[0]-ssi_start >= 1999);
  CHECK(mocksensor::ssi_reads[0] == mocksensor::ssi_reads[1]);
  CHECK(h.ready() && h.motionSnapshot()->adc_stats.errors == 0);
  CHECK(h.motionSnapshot()->adc_stats.skipped_periods == 0);
  return 0;
}
extern "C" int test_app_sensor_cost_and_stall_no_catchup() {
  MemoryFlash flash; app_flash = &flash;
  CHECK(usb::normalHid().configureBeforeConnect());
  mocksensor::enabled = true;
  app::App h; h.beginCore0(); h.beginCore1(); tickApp(h,150);
  // 外设执行消耗真实时间，不能靠零成本替身才达到目标。
  mocksensor::adc_cost_us = 4; mocksensor::ssi_cost_us = 16;
  const uint32_t cost_start = mockapp::now;
  const uint32_t cost_adc_start = mocksensor::adc_reads;
  while (mockapp::now-cost_start < 200000) {
    mockapp::now += 37;
    h.serviceCore1(); h.serviceCore0();
  }
  const uint32_t cost_vectors = (mocksensor::adc_reads-cost_adc_start)/3;
  CHECK(cost_vectors >= 199 && cost_vectors <= 201);
  CHECK(h.motionSnapshot()->adc_stats.skipped_periods == 0);
  CHECK(h.motionSnapshot()->adc_stats.max_span_us == 12);
  const uint32_t stalled_reads = mocksensor::adc_reads;
  mockapp::now += 10000;
  h.serviceCore1();
  CHECK(mocksensor::adc_reads-stalled_reads == 3);
  h.serviceCore1();
  CHECK(mocksensor::adc_reads-stalled_reads == 3);
  return 0;
}
static hall::Sample at(float mm, const hall::Calibration &c, uint32_t seq, uint32_t time) {
  hall::Sample s{};
  s.sequence = seq;
  s.timestamp_us = time;
  uint8_t j = 0;
  while (j + 2 < c.node_count && mm > c.nodes[j + 1].mm)
    ++j;
  const hall::Node &a = c.nodes[j], &b = c.nodes[j + 1];
  const float t = hall::clamp((mm - a.mm) / (b.mm - a.mm), 0, 1);
  for (uint8_t i = 0; i < 3; ++i) {
    const float r = a.response[i] + t * (b.response[i] - a.response[i]);
    s.raw[i] = static_cast<uint16_t>(c.baseline[i] +
                                     r / (c.polarity[i] * c.gain[i]) + 0.5f);
  }
  return s;
}

static void sensorAt(float mm,const hall::Calibration &map){const auto s=at(mm,map,0,0);for(uint8_t i=0;i<3;++i)mocksensor::raw[i]=s.raw[i];}
static uint16_t cachedY() {
  uint8_t bytes[5]{};
  __usb_mutex.locked=true;
  const uint16_t size=usb::normalHid().getMouse(2,HID_REPORT_TYPE_INPUT,bytes,5);
  __usb_mutex.locked=false;
  return size==5 ? uint16_t(bytes[3]|(uint16_t(bytes[4])<<8)) : 65535;
}
static void tickUsb1ms(app::App &h,uint32_t loops) {
  for(uint32_t i=0;i<loops;++i) {
    mockapp::now+=500;
    mockapp::complete_usb=(i&1)!=0;
    h.serviceCore1(); h.serviceCore0();
  }
  mockapp::complete_usb=true;
}
extern "C" int test_app_absolute_xy_y_knob_hall_save_and_profile_resume() {
  MemoryFlash flash; app_flash=&flash;
  CHECK(usb::normalHid().configureBeforeConnect()); mocksensor::enabled=true;
  app::App h; h.beginCore0(); h.beginCore1(); tickApp(h,150); CHECK(h.ready());
  const auto map=fixture(); auto device=*h.deviceConfig();
  device.calibration=map; device.calibration_generation=1; device.revision=2;
  auto *p=config::find(device,device.active_id);
  p->hall.enabled=true; p->hall.quiet_tau_us=0; p->knobs[1].enabled=true;
  p->knobs[1].axis=knobs::Axis::Y;
  sensorAt(-30,map); CHECK(h.applyDevice(device,mockapp::now)); tickApp(h,150);
  CHECK(cachedY()==usb::kAbsoluteAxisCenter && h.motionSnapshot()->hall.valid);
  mocksensor::advance_ticks[1]=16; tickUsb1ms(h,240);
  mocksensor::advance_ticks[1]=0; tickUsb1ms(h,100);
  const uint16_t y=cachedY(); CHECK(y>usb::kAbsoluteAxisCenter && y<32767);
  CHECK(h.ready() && h.routingStats().queue_full==0);
  CHECK(mockusb::endpoints[1].payload[0]==2);
  sensorAt(30,map); tickApp(h,30); CHECK(cachedY()==y);
  CHECK(h.save(mockapp::now)); tickApp(h,150);
  CHECK(h.ready() && h.saveResult()==config::SaveResult::Ok && cachedY()==y);
  mockusb::mounted=false; tickApp(h,30);
  mockusb::mounted=true; tickApp(h,150); CHECK(h.ready() && cachedY()==y);
  device=*h.deviceConfig(); device.revision=config::nextRevision(device.revision);
  config::find(device,device.active_id)->hall.enabled=false;
  CHECK(h.applyDevice(device,mockapp::now)); tickApp(h,30);
  mocksensor::advance_ticks[1]=16; tickApp(h,20);
  CHECK(mockusb::endpoints[1].payload[0]==1 && cachedY()==y);
  mocksensor::advance_ticks[1]=0;
  device=*h.deviceConfig(); device.revision=config::nextRevision(device.revision);
  config::find(device,device.active_id)->hall.enabled=true;
  CHECK(h.applyDevice(device,mockapp::now)); tickApp(h,30);
  CHECK(cachedY()==y && mockusb::endpoints[1].payload[0]==2);
  return 0;
}
extern "C" int test_app_absolute_xy_async_failure_and_reconnect_without_hall() {
  MemoryFlash flash; app_flash=&flash;
  CHECK(usb::normalHid().configureBeforeConnect()); mocksensor::enabled=true;
  app::App h; h.beginCore0(); h.beginCore1(); tickApp(h,150); CHECK(h.ready());
  auto device=*h.deviceConfig(); device.revision=2;
  auto *p=config::find(device,device.active_id);
  p->hall.enabled=true; p->knobs[1].enabled=true; p->knobs[1].axis=knobs::Axis::Y;
  CHECK(h.applyDevice(device,mockapp::now)); tickApp(h,150);
  mocksensor::advance_ticks[1]=16; tickApp(h,25);
  mocksensor::advance_ticks[1]=0; tickApp(h,100);
  const uint16_t y=cachedY();
  CHECK(y>usb::kAbsoluteAxisCenter && !h.motionSnapshot()->hall.valid);
  const auto recoveries=h.stats().recoveries;
  __usb_mutex.locked=true;
  usb::usbBackend().failed(1);
  __usb_mutex.locked=false;
  auto &e=mockusb::endpoints[1]; e.ready=true; e.payload[0]=0x7f;
  tickApp(h,150);
  CHECK(h.ready() && h.stats().recoveries>recoveries && cachedY()==y);
  CHECK(e.payload[0]==2 && uint16_t(e.payload[4]|(uint16_t(e.payload[5])<<8))==y);
  mockusb::mounted=false; tickApp(h,30);
  e.payload[0]=0x7f;
  mockusb::mounted=true; tickApp(h,150);
  CHECK(h.ready() && cachedY()==y && e.payload[0]==2);
  return 0;
}
extern "C" int test_app_full_calibration_adopt_save_and_boot_reuse(){
  MemoryFlash flash;app_flash=&flash;CHECK(usb::normalHid().configureBeforeConnect());mocksensor::enabled=true;
  app::App h;h.beginCore0();h.beginCore1();tickApp(h,150);CHECK(h.ready());
  common::CalibrationCommand c;c.op=common::CalibrationOp::Start;c.token=100;c.job_id=202;c.magnet_removed=true;c.left_mm=-41;c.right_mm=39;
  CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,560);CHECK(h.calibrationSucceeded());
  auto map=fixture();
  for(uint8_t i=0;i<3;++i){sensorAt(hall::kSensorMm[i],map);c.op=common::CalibrationOp::Reference;c.channel=i;++c.token;CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,290);CHECK(h.calibrationSucceeded());}
  for(uint8_t i=0;i<map.node_count;++i){sensorAt(map.nodes[i].mm,map);c.op=common::CalibrationOp::Node;c.mm=map.nodes[i].mm;++c.token;CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,290);CHECK(h.calibrationSucceeded());}
  c.op=common::CalibrationOp::BeginSweep;++c.token;CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,220);CHECK(h.motionSnapshot()->calibration.stage==hall::CalibrationStage::Sweep);
  for(uint8_t i=0;i<map.node_count;++i){sensorAt(map.nodes[i].mm,map);tickApp(h,4);if(i+1<map.node_count){sensorAt((map.nodes[i].mm+map.nodes[i+1].mm)*0.5f,map);tickApp(h,4);}}
  c.op=common::CalibrationOp::Finish;++c.token;CHECK(h.calibrationCommand(c,mockapp::now));tickApp(h,150);
  CHECK(h.calibrationSucceeded()&&!h.calibrationPending()&&!h.calibrating()&&h.deviceConfig()->calibration.valid&&h.deviceConfig()->revision==2&&h.deviceConfig()->calibration_generation==1);
  auto device=*h.deviceConfig();auto *profile=config::find(device,device.active_id);profile->hall.enabled=true;profile->hall.quiet_tau_us=0;device.revision=3;CHECK(h.applyDevice(device,mockapp::now));sensorAt(0,map);tickApp(h,150);
  CHECK(h.motionSnapshot()->hall.valid&&h.motionSnapshot()->hall.output>0.49f&&h.motionSnapshot()->hall.output<0.53f);
  CHECK(h.save(mockapp::now));tickApp(h,150);CHECK(h.savedRevision()==3&&h.saveResult()==config::SaveResult::Ok);
  app::App boot;CHECK(boot.prepareUsbBoot(false)==0&&boot.deviceConfig()->calibration.valid);boot.beginCore0();boot.beginCore1();tickApp(boot,150);
  CHECK(boot.ready()&&boot.motionSnapshot()->hall.valid&&boot.motionSnapshot()->calibration.stage==hall::CalibrationStage::Idle);return 0;
}

extern "C" int test_app_cdc_motion_calibration_and_uf2_delivery_gate(){
  MemoryFlash flash;app_flash=&flash;CHECK(usb::normalHid().configureBeforeConnect());mocksensor::enabled=true;
  app::App h;h.beginCore0();h.beginCore1();tickApp(h,150);protocol::ConfigProtocol protocol;Port port;
  request(port,1,"hello");for(uint16_t i=0;i<120;++i){tickApp(h,1);protocol.service(mockapp::now,port,h);}
  CHECK(contains(port.tx,"\"scope\":\"device_schema2\"")&&contains(port.tx,"\"hall\":true")&&contains(port.tx,"\"bootloader_command\":true"));
  port.clearOutput();request(port,2,"get_motion");for(uint16_t i=0;i<120;++i){tickApp(h,1);protocol.service(mockapp::now,port,h);}
  CHECK(contains(port.tx,"\"raw_adc\":[2048,2048,2048]")&&contains(port.tx,"\"knobs\":[")&&contains(port.tx,"\"sample_ready\":true"));
  CHECK(contains(port.tx,"\"adc_attempts\":")&&contains(port.tx,"\"skipped_periods\":0")&&contains(port.tx,"\"max_lateness_us\":")&&contains(port.tx,"\"max_span_us\":0"));
  CHECK(h.bootloader(mockapp::now));tickApp(h,30);CHECK(h.rebootReady()&&!TinyUSBDevice.detached&&mockapp::bootloaders==0);
  h.rebootResponseComplete(false,mockapp::now);tickApp(h,30);CHECK(h.ready()&&!TinyUSBDevice.detached&&mockapp::bootloaders==0);
  CHECK(h.bootloader(mockapp::now));tickApp(h,30);h.rebootResponseComplete(true,mockapp::now);CHECK(TinyUSBDevice.detached&&mockapp::bootloaders==0);
  mockapp::now+=249000;h.serviceCore0();CHECK(mockapp::bootloaders==0);mockapp::now+=1000;h.serviceCore0();CHECK(mockapp::bootloaders==1&&rp2040.reboots==0);return 0;
}
extern "C" int test_app_actual_hall_to_io4_adc_and_no_spinner(){
  MemoryFlash flash;app_flash=&flash;CHECK(usb::usbBackend().configureBeforeConnect(1));mocksensor::enabled=true;
  auto map=fixture();common::DeviceConfig saved;config::defaults(saved);saved.startup_usb_mode=1;saved.calibration=map;saved.calibration_generation=1;auto *p=config::find(saved,saved.active_id);p->hall.enabled=true;p->hall.quiet_tau_us=0;
  config::ConfigStore store(flash);CHECK(store.save(saved)==config::SaveResult::Ok);sensorAt(39,map);
  app::App h;CHECK(h.prepareUsbBoot(false)==1);h.beginCore0();h.beginCore1();tickApp(h,150);CHECK(h.ready()&&h.motionSnapshot()->hall.valid);
  const auto &e=mockusb::endpoints[0];CHECK(e.length==64&&e.payload[0]==1&&e.payload[1]==255&&e.payload[2]==255);
  for(uint8_t i=17;i<25;++i) { CHECK(e.payload[i]==0); }
  CHECK(h.routingStats().submitted[0]==0&&h.routingStats().submitted[1]==0);return 0;
}
