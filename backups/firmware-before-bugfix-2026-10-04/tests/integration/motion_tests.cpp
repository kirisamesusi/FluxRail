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
  CHECK(u.tryAbsolute(0.75f));const auto &e=mockusb::endpoints[1];CHECK(e.length==3&&e.payload[0]==2);TinyUSB_Device_Task();
  CHECK(u.tryRelative(-12,31));CHECK(e.length==6&&e.payload[0]==1&&e.payload[1]==5&&e.payload[2]==244&&e.payload[4]==31);
  uint8_t bytes[2];CHECK(usb::encodeAbsoluteX(0,bytes)&&bytes[0]==0&&bytes[1]==0);CHECK(usb::encodeAbsoluteX(1,bytes)&&bytes[0]==255&&bytes[1]==127);
  CHECK(!usb::encodeAbsoluteX(-1,bytes)&&!usb::encodeAbsoluteX(2,bytes));return 0;
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
