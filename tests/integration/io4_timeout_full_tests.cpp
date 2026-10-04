#include "io4_tests.cpp"
extern "C" int test_io4_future_command_timestamp_must_not_expire() {
    const uint8_t units[] = {1,4,255};
    for (auto u:units) {
        fluxrail::usb::Io4State s;fluxrail::usb::Io4Output cmd;
        cmd.command=1;cmd.payload[0]=u;
        CHECK(s.accept(cmd,1001));
        s.tick(1000); // IRQ arrived after the caller cached its loop time.
        CHECK(s.input.system==0x30 && s.timeout_units==u && s.timeouts==0);
        s.tick(1001+uint32_t(u)*100000u-1u);CHECK(s.input.system==0x30);
        s.tick(1001+uint32_t(u)*100000u);CHECK(s.input.system==2 && s.timeouts==1);
    }
    return 0;
}
extern "C" int test_io4_timeout_wrap_and_disabled_boundaries() {
    fluxrail::usb::Io4State s;fluxrail::usb::Io4Output cmd;
    cmd.command=1;cmd.payload[0]=2;
    CHECK(s.accept(cmd,UINT32_MAX-50000u));
    s.tick(UINT32_MAX-50001u);CHECK(s.input.system==0x30 && s.timeouts==0);
    s.tick(149998u);CHECK(s.input.system==0x30);
    s.tick(149999u);CHECK(s.input.system==2 && s.timeouts==1);
    cmd.payload[0]=0;CHECK(s.accept(cmd,200000u));
    s.tick(199999u);s.tick(30000000u);CHECK(s.input.system==0x30 && s.timeouts==1);
    return 0;
}
extern "C" int test_io4_later_commands_preserve_timer_before_sample() {
    fluxrail::usb::Io4State s;fluxrail::usb::Io4Output cmd;
    cmd.command=1;cmd.payload[0]=4;CHECK(s.accept(cmd,1000));
    cmd.command=2;CHECK(s.accept(cmd,1100));s.tick(1099);
    CHECK(s.input.system==0x30 && s.timeout_units==4 && s.timeouts==0);
    cmd.command=3;CHECK(s.accept(cmd,1200));s.tick(1199);
    CHECK(s.input.system==0 && s.timeout_units==4 && s.timeouts==0);
    cmd.command=4;cmd.payload[0]=0;CHECK(s.accept(cmd,1300));s.tick(1299);
    CHECK(s.gpio_active && s.timeouts==0);
    s.tick(401299);CHECK(s.gpio_active && s.timeouts==0);
    s.tick(401300);CHECK(!s.gpio_active && s.input.system==2 && s.timeouts==1);
    return 0;
}
