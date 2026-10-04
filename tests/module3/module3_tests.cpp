#include "knobs/KnobModule.h"
#include "knobs/KnobSampler.h"
#include "knobs/AppPushSink.h"
#include <stddef.h>
using namespace fluxrail::knobs;
// 原始C++以ARM指令运行；合成帧只用于算法验证，不模拟实际SSI/双核/USB。
extern "C" void *memset(void *dst, int value, size_t count) {
  auto p = static_cast<unsigned char *>(dst);
  for (size_t i = 0; i < count; ++i) p[i] = static_cast<unsigned char>(value);
  return dst;
}
extern "C" void *memcpy(void *dst, const void *src, size_t count) {
  auto p = static_cast<unsigned char *>(dst);
  auto q = static_cast<const unsigned char *>(src);
  for (size_t i = 0; i < count; ++i) p[i] = q[i];
  return dst;
}
void operator delete(void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() { for (;;) {} }
volatile uint32_t checks = 0;
extern "C" const uint32_t knob_layout[6] = {
  sizeof(Sample), sizeof(Motion), sizeof(Settings), sizeof(KnobModule), sizeof(KnobMotion), sizeof(PushFilter)
};
#define CHECK(x) do { ++checks; if (!(x)) return __LINE__; } while (0)
static bool near(double a, double b, double eps = 0.000001) { return a-b < eps && b-a < eps; }
// CRC参考采用24位多项式长除法，与驱动6位移位寄存器实现独立。
static uint8_t referenceCrc(uint32_t payload) {
  uint32_t dividend = payload << 6;
  for (int bit = 23; bit >= 6; --bit)
    if (dividend & (1u << bit)) dividend ^= 0x43u << (bit - 6);
  return static_cast<uint8_t>(dividend);
}
static Sample frame(uint16_t angle, uint32_t now, uint32_t seq, uint8_t status = 0, bool gpio = false) {
  Sample s{};
  const uint32_t payload = (static_cast<uint32_t>(angle) << 4) | status;
  s.frame = (payload << 6) | referenceCrc(payload);
  s.timestamp_us = now; s.sequence = seq; s.transport_ok = true; s.push_gpio = gpio;
  return s;
}
static Settings enabled(uint8_t id = 0) { Settings s=defaults(id); s.enabled=true; return s; }
extern "C" int test_crc_and_status() {
  CHECK(crc6(0) == 0); CHECK(crc6(1) == 3); CHECK(crc6(0x20000) == referenceCrc(0x20000));
  for (uint32_t p = 0; p < 0x40000; p += 127) CHECK(crc6(p) == referenceCrc(p));
  for (uint8_t status = 0; status < 16; ++status) {
    auto s=frame(12345,500,0,status,(status&4)!=0); auto f=decode(s);
    CHECK(f.crc_ok && f.angle==12345 && f.status==status && f.push==((status&4)!=0));
    CHECK(f.field_ok == ((status&11)==0));
  }
  auto s=frame(8191,0,0);
  for (uint8_t bit = 0; bit < 24; ++bit) { auto bad=s; bad.frame^=1u<<bit; CHECK(!decode(bad).crc_ok); }
  s.frame |= 1u<<24; CHECK(!decode(s).crc_ok);
  s=frame(123,0,0); s.transport_ok=false; CHECK(decode(s).quality==Quality::TransportError);
  return 0;
}
extern "C" int test_wrap_multiturn_and_gain() {
  for (uint8_t scale=0;scale<3;++scale) {
    KnobMotion m; auto s=enabled(); s.gain=scale==0?0.5:(scale==1?1:2);
    CHECK(m.apply(s,1)); CHECK(!m.sample(frame(16000,0,0),0,1).valid);
    uint16_t a=16000; double sum=0; Motion out{};
    for(uint32_t i=1;i<=192;++i) { a=(a+256)&16383;out=m.sample(frame(a,i*500,i),0,1);CHECK(out.valid&&out.delta_ticks==256);sum+=out.delta_counts; }
    CHECK(near(out.raw_turns,3)); CHECK(near(out.scaled_turns,3*s.gain)); CHECK(near(sum,3072*s.gain));
    for(uint32_t i=193;i<=384;++i) { a=(a-256)&16383;out=m.sample(frame(a,i*500,i),0,1);CHECK(out.valid&&out.delta_ticks==-256);sum+=out.delta_counts; }
    CHECK(near(out.raw_turns,0)); CHECK(near(out.scaled_turns,0)); CHECK(near(sum,0));
  }
  KnobMotion m; auto s=enabled(1); s.invert=true; CHECK(m.apply(s,3));
  m.sample(frame(0,0,0),1,9); auto out=m.sample(frame(1,500,1),1,9);
  CHECK(out.axis==Axis::Y&&out.revision==3&&out.epoch==9&&out.knob_id==1);
  CHECK(near(out.delta_counts,-1.0/16)); CHECK(near(out.scaled_turns,-1.0/16384));
  return 0;
}
extern "C" int test_crc_gap_and_field_recovery() {
  KnobMotion m; CHECK(m.apply(enabled(),1)); m.sample(frame(16000,0,0),0,1);
  auto bad=frame(16100,500,1); bad.frame^=1; CHECK(!m.sample(bad,0,1).valid);
  auto out=m.sample(frame(16200,1000,2),0,1); CHECK(out.valid&&out.delta_ticks==200);
  for(uint8_t status=1;status<=15;++status) if(status!=4) {
    CHECK(!m.sample(frame(12000,1500+status*1000,status*2+1,status),0,1).valid);
    CHECK(!m.sample(frame(100,2000+status*1000,status*2+2),0,1).valid);
  }
  CHECK(m.stats().strong_field>0&&m.stats().weak_field>0&&m.stats().loss_of_track>0&&m.stats().reserved_field>0);
  m.resetReference(); CHECK(!m.sample(frame(100,100000,100),0,1).valid);
  out=m.sample(frame(110,104000,101),0,1); CHECK(!out.valid&&out.quality==Quality::Gap&&out.delta_counts==0);
  out=m.sample(frame(120,104500,102),0,1); CHECK(out.valid&&out.delta_ticks==10);
  bad=frame(123,105000,103);bad.transport_ok=false; CHECK(!m.sample(bad,0,1).valid);
  CHECK(!m.sample(frame(9000,105500,104),0,1).valid);
  out=m.sample(frame(9001,106000,106),0,1); CHECK(!out.valid&&out.quality==Quality::Gap);
  CHECK(m.stats().crc_errors==1&&m.stats().transport_errors==1&&m.stats().sequence_gaps>0);
  return 0;
}
extern "C" int test_ambiguity_speed_and_time_wrap() {
  auto s=enabled();s.max_turns_per_second=1000; KnobMotion m; CHECK(m.apply(s,1));
  m.sample(frame(0,0,0),0,1);
  auto out=m.sample(frame(8192,500,1),0,1);CHECK(!out.valid&&out.quality==Quality::Ambiguous);
  out=m.sample(frame(8193,1000,2),0,1);CHECK(!out.valid&&out.quality==Quality::Ambiguous); // 允许转速*dt已>=半圈。
  s.max_turns_per_second=1;CHECK(m.apply(s,2));m.sample(frame(0,0,0),0,1);
  CHECK(m.sample(frame(9,500,1),0,1).quality==Quality::Ambiguous); // 可信速度边界8.192ticks。
  CHECK(m.apply(enabled(),3));m.sample(frame(16380,0xfffffff0u,0xffffffffu),0,1);
  out=m.sample(frame(4,484,0),0,1);CHECK(out.valid&&out.delta_ticks==8);
  CHECK(m.stats().max_interval_us>=500);CHECK(out.raw_ticks==8);
  out=m.sample(frame(5,484,1),0,1);CHECK(!out.valid&&out.quality==Quality::Gap);
  return 0;
}
extern "C" int test_fraction_filter_activity_and_config() {
  KnobMotion m; auto s=enabled();s.activity_ticks=8;CHECK(m.apply(s,1));m.sample(frame(0,0,0),0,1);
  Motion out{};double sum=0;
  for(uint32_t i=1;i<=16;++i){out=m.sample(frame(i,i*500,i),0,1);sum+=out.delta_counts;CHECK(out.valid);if(i<8)CHECK(!out.active);}
  CHECK(out.active&&near(sum,1)); // 每步1/16 count不能被逐帧舍弃。
  s.gain=2;CHECK(m.apply(s,2));out=m.sample(frame(8000,10000,100),0,2);
  CHECK(!out.valid&&out.delta_counts==0&&near(out.raw_turns,16.0/16384));
  out=m.sample(frame(8001,10500,101),0,2);CHECK(near(out.delta_counts,0.125));
  s.filter_tau_us=1500;CHECK(m.apply(s,3));m.sample(frame(0,0,0),0,3);
  out=m.sample(frame(16,500,1),0,3);CHECK(near(out.delta_counts,0.5));sum=out.delta_counts;
  for(uint32_t i=2;i<100;++i){out=m.sample(frame(16,i*500,i),0,3);sum+=out.delta_counts;}
  CHECK(near(sum,2,0.00001));CHECK(!out.active); // 滤波尾量最终守恒，活动不由滤波伪造。
  out=m.sample(frame(32,50000,100),0,3);CHECK(out.valid);m.resetReference();CHECK(m.stats().filter_discarded_ticks>0);
  CHECK(!m.sample(frame(1000,50500,101),0,4).valid);
  s.enabled=false;CHECK(m.apply(s,4));m.sample(frame(0,0,0),0,4);out=m.sample(frame(16,500,1),0,4);CHECK(out.valid&&out.delta_counts==0&&!out.enabled);
  return 0;
}
extern "C" int test_push_start_rotate_release_timeout() {
  PushFilter p; PushSettings cfg;cfg.enabled=true;p.apply(cfg);
  uint32_t seq=0;
  for(uint32_t t=0;t<=6000;t+=500)CHECK(!p.sample(frame(0,t,seq++,4,true),false)); // 启动已高不制造按下。
  for(uint32_t t=6500;t<=9000;t+=500)CHECK(!p.sample(frame(0,t,seq++),false));
  for(uint32_t t=9500;t<14500;t+=500)CHECK(!p.sample(frame(0,t,seq++,4,true),true));
  CHECK(p.sample(frame(0,14500,seq++,4,true),true)); // 旋转仍接受可靠稳定按压。
  CHECK(p.sample(frame(0,15000,seq++),true));CHECK(p.sample(frame(0,15500,seq++),true));
  CHECK(!p.sample(frame(0,16000,seq++),true));CHECK(p.stats().presses==1&&p.stats().releases==1);
  // 超时不重复产生DOWN，保持高必须回低再武装。
  cfg.max_hold_us=3000;p.apply(cfg);seq=0;
  for(uint32_t t=0;t<=2500;t+=500)p.sample(frame(0,t,seq++),false);
  for(uint32_t t=3000;t<=5000;t+=500)p.sample(frame(0,t,seq++,4,true),false);
  CHECK(p.down());for(uint32_t t=5500;t<=9000;t+=500)p.sample(frame(0,t,seq++,4,true),false);
  CHECK(!p.down()&&p.stats().hold_timeouts==1);
  for(uint32_t t=9500;t<=15000;t+=500)CHECK(!p.sample(frame(0,t,seq++,4,true),false));
  return 0;
}
extern "C" int test_push_quality_mismatch_and_gap() {
  PushFilter p; PushSettings cfg;cfg.enabled=true;p.apply(cfg);uint32_t seq=0;
  for(uint32_t t=0;t<=2500;t+=500)p.sample(frame(0,t,seq++),false);
  CHECK(!p.sample(frame(0,3000,seq++,0,true),false)); // GPIO单独高不能按下。
  CHECK(p.stats().mismatches==1);
  CHECK(!p.sample(frame(0,3500,seq++,4,true),false));
  CHECK(!p.sample(frame(0,4000,seq++),false));CHECK(p.stats().rejected_candidates==1);
  for(uint32_t t=4500;t<=6500;t+=500)p.sample(frame(0,t,seq++,4,true),false);
  CHECK(p.down());auto bad=frame(0,7000,seq++,4,true);bad.frame^=1;
  CHECK(!p.sample(bad,false));CHECK(p.stats().quality_releases==1);
  for(uint32_t t=7500;t<=10000;t+=500)p.sample(frame(0,t,seq++),false);
  p.sample(frame(0,10500,seq++,4,true),false);
  CHECK(!p.sample(frame(0,14000,seq++,4,true),false)); // 缺采不接续确认。
  for(uint32_t t=14500;t<=19000;t+=500)CHECK(!p.sample(frame(0,t,seq++,4,true),false));
  cfg.enabled=false;p.apply(cfg);CHECK(!p.sample(frame(0,20000,seq++,4,true),false));
  // 时间/序号回绕也能正确稳定武装。
  cfg.enabled=true;p.apply(cfg);seq=0xfffffff0u;
  for(uint32_t i=0;i<6;++i)p.sample(frame(0,0xffffff00u+i*500,seq++),false);
  for(uint32_t i=6;i<11;++i)p.sample(frame(0,0xffffff00u+i*500,seq++,4,true),false);
  CHECK(p.down());return 0;
}
struct Sink final : PushSink {
  bool states[2]{};uint32_t calls[2]{};bool accept=true;
  bool submitFilteredPush(uint8_t id,bool down,uint32_t) override {
    if(id<10||id>11)return false;
    states[id-10]=down;++calls[id-10];return accept;
  }
};
extern "C" int test_dual_queues_order_backpressure_and_resume() {
  KnobModule m; Sink sink;Settings s[2]={enabled(),enabled(1)};Discarded discarded[2]{};
  CHECK(m.apply(s,1));CHECK(m.resume(1,discarded));Motion a,b;
  for(uint32_t i=0;i<40;++i){m.sample(0,frame(i%2?100:0,i*500,i),sink);m.sample(1,frame((i*200)&16383,i*500,i),sink);
    CHECK(m.tryTake(0,a)&&m.tryTake(1,b));CHECK(a.axis==Axis::X&&b.axis==Axis::Y);
    if(i){CHECK(a.delta_ticks==(i%2?100:-100));CHECK(b.delta_ticks==200);}
  }
  // 一路队列满，另一轴继续采样和消费；快速往返不压成总和零。
  for(uint32_t i=40;i<105;++i){m.sample(0,frame(i%2?100:0,i*500,i),sink);m.sample(1,frame((i*200)&16383,i*500,i),sink);CHECK(m.tryTake(1,b));}
  CHECK(m.paused(0)&&!m.paused(1)&&m.recoveryNeeded(0));CHECK(!m.tryTake(0,a));
  CHECK(m.stats().queue_full[0]==1&&m.stats().queue_full[1]==0);
  CHECK(!m.apply(s,2));m.pause(sink,53000);CHECK(!sink.states[0]&&!sink.states[1]);
  CHECK(!m.resume(2,discarded));discarded[0]=m.discardPaused(0);discarded[1]=m.discardPaused(1);
  CHECK(discarded[0].records==64&&discarded[1].records==0);CHECK(m.apply(s,2));CHECK(!m.resume(1,discarded));CHECK(m.resume(2,discarded));
  m.sample(0,frame(8000,54000,106),sink);CHECK(m.tryTake(0,a)&&a.delta_counts==0&&!a.valid&&a.epoch==2&&a.revision==2);
  CHECK(m.stats().recovery_discarded[0]==65);CHECK(discarded[0].absolute_counts>0);
  CHECK(near(m.stats().recovery_discarded_absolute_counts[0],discarded[0].absolute_counts+6.25));CHECK(!m.recoveryNeeded(0));
  sink.accept=false;m.sample(1,frame(2,54000,106),sink);CHECK(m.stats().push_submit_failures==1);
  return 0;
}
struct FakeReader final : Reader {
  uint32_t calls[2]{}; bool read(uint8_t id, Sample &s) override {++calls[id];s.frame=0;return id==1;}
};
extern "C" int test_sampler_independence_and_skips() {
  FakeReader r;KnobSampler sampler;Sample a,b;
  CHECK(sampler.poll(0,0,r,a)&&sampler.poll(1,0,r,b));CHECK(!a.transport_ok&&b.transport_ok);
  CHECK(!sampler.poll(0,499,r,a));CHECK(sampler.poll(1,500,r,b));CHECK(sampler.poll(0,500,r,a));
  CHECK(a.sequence==1&&b.sequence==1);
  CHECK(sampler.poll(0,3000,r,a));CHECK(sampler.stats().skipped_periods[0]==4);
  CHECK(!sampler.poll(0,3000,r,a));CHECK(sampler.poll(1,3000,r,b));CHECK(r.calls[0]==3&&r.calls[1]==3);
  CHECK(!sampler.poll(2,3000,r,a));sampler.reset();CHECK(sampler.poll(0,3010,r,a)&&a.sequence==3);
  return 0;
}
extern "C" int test_sampler_phase_jitter_and_wrap() {
  FakeReader r; KnobSampler sampler; Sample s;
  uint32_t counts[2]{};
  const uint32_t start = 0xffff8000u;
  for (uint32_t dt = 0; dt < 100000; dt += 97) {
    for (uint8_t id = 0; id < 2; ++id)
      if (sampler.poll(id, start + dt, r, s)) ++counts[id];
  }
  CHECK(counts[0] == 200 && counts[1] == 200);
  CHECK(sampler.stats().skipped_periods[0] == 0);
  CHECK(sampler.poll(0, start + 102700, r, s));
  CHECK(sampler.stats().skipped_periods[0] == 5);
  CHECK(!sampler.poll(0, start + 102700, r, s));
  CHECK(sampler.poll(0, start + 103000, r, s));
  return 0;
}
extern "C" int test_validation_atomic_config() {
  auto s=enabled();CHECK(validate(s));s.gain=0;CHECK(!validate(s));s=enabled();s.counts_per_turn_at_gain1=1000001;CHECK(!validate(s));
  s=enabled();s.max_turns_per_second=0;CHECK(!validate(s));s=enabled();s.axis=static_cast<Axis>(2);CHECK(!validate(s));
  s=enabled();s.push.rotating_press_us=1;CHECK(!validate(s));s=enabled();s.max_gap_us=499;CHECK(!validate(s));
  // 直接位型构造NaN/Inf，避免测试宿主math库与目标C++计算不一致。
  union Bits { uint64_t bits;double value; } bad{};bad.bits=0x7ff8000000000000ULL;s=enabled();s.gain=bad.value;CHECK(!validate(s));
  bad.bits=0x7ff0000000000000ULL;s.gain=bad.value;CHECK(!validate(s));
  KnobModule m;Settings pair[2]={enabled(),enabled(1)};CHECK(m.apply(pair,5));pair[0].gain=2;pair[1].gain=0;CHECK(!m.apply(pair,6));
  Sink sink;Discarded dropped[2]{};CHECK(m.resume(1,dropped));m.sample(0,frame(0,0,0),sink);m.sample(0,frame(16,500,1),sink);
  Motion out;CHECK(m.tryTake(0,out));CHECK(m.tryTake(0,out));CHECK(out.revision==5&&near(out.delta_counts,1));return 0;
}

struct FakeApp {
  uint8_t id=0;bool state=false;uint32_t timestamp=0;bool accepted=false;
  bool submitFilteredPush(uint8_t source,bool down,uint32_t now) {id=source;state=down;timestamp=now;return accepted;}
};
extern "C" int test_public_push_adapter() {
  FakeApp app;AppPushSink<FakeApp> sink(app);
  CHECK(!sink.submitFilteredPush(10,true,123));CHECK(app.id==10&&app.state&&app.timestamp==123);
  app.accepted=true;CHECK(sink.submitFilteredPush(11,false,234));CHECK(app.id==11&&!app.state&&app.timestamp==234);
  return 0;
}

extern "C" int test_single_bad_sensor_and_same_axis() {
  KnobModule m;Sink sink;Settings settings[2]={enabled(),enabled(1)};
  settings[1].axis=Axis::X; // 尚未定义同轴合并；模块必须保留来源，不能擅自相加/择一。
  CHECK(m.apply(settings,1));Discarded discarded[2]{};CHECK(m.resume(1,discarded));
  Motion a,b;double produced=0;
  for(uint32_t i=0;i<100;++i) {
    auto bad=frame((i*50)&16383,i*500,i);bad.frame^=1;
    m.sample(0,bad,sink);m.sample(1,frame((i*100)&16383,i*500,i),sink);
    CHECK(m.tryTake(0,a)&&m.tryTake(1,b));CHECK(!a.valid&&a.delta_counts==0);
    CHECK(a.knob_id==0&&b.knob_id==1&&a.axis==b.axis);
    if(i) {CHECK(b.valid&&b.delta_ticks==100);produced+=b.delta_counts;}
  }
  CHECK(near(produced,9900.0/16));CHECK(m.motionStats(0).crc_errors==100&&m.motionStats(1).crc_errors==0);
  m.sample(0,frame(0,50000,100,4,true),sink);
  CHECK(m.latestPush(0).raw_gpio&&m.latestPush(0).ssi_push&&!m.latestPush(0).filtered_down);
  CHECK(m.latestPush(0).source_id==10&&m.latestPush(0).revision==1&&m.latestPush(0).epoch==1);
  return 0;
}

extern "C" int test_push_snapshot_on_pause_and_overflow() {
  KnobModule m;Sink sink;Settings settings[2]={enabled(),enabled(1)};
  settings[0].push.enabled=true;CHECK(m.apply(settings,1));Discarded dropped[2]{};CHECK(m.resume(1,dropped));
  for(uint32_t i=0;i<64;++i)m.sample(0,frame(0,i*500,i,i>=6?4:0,i>=6),sink);
  CHECK(sink.states[0]&&m.latestPush(0).filtered_down);
  m.sample(0,frame(0,32000,64,4,true),sink);
  CHECK(m.paused(0)&&!sink.states[0]&&!m.latestPush(0).filtered_down);
  CHECK(m.latestPush(0).raw_gpio&&m.latest(0).quality==Quality::Suspended&&m.latest(0).delta_counts==0);
  m.pause(sink,32500);dropped[0]=m.discardPaused(0);dropped[1]=m.discardPaused(1);
  CHECK(m.resume(2,dropped));
  for(uint32_t i=65;i<71;++i)m.sample(0,frame(0,i*500,i),sink);
  for(uint32_t i=71;i<77;++i)m.sample(0,frame(0,i*500,i,4,true),sink);
  CHECK(m.latestPush(0).filtered_down&&sink.states[0]);m.pause(sink,39000);
  CHECK(!m.latestPush(0).filtered_down&&!sink.states[0]&&!m.latest(0).valid);
  return 0;
}
