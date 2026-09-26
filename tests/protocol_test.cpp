#include "test_harness.h"

#include "lamp/core/protocol.h"

#include <string>

using namespace lamp;

TEST(build_channel_commands) {
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetChannel, 1, 153, "", 0}),
              std::string("SPA153#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetChannel, 2, 0, "", 0}),
              std::string("SPB0#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetChannel, 3, 100, "", 0}),
              std::string("SPC100#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetChannel, 4, 255, "", 0}),
              std::string("SPD255#"));
}

TEST(build_parameter_commands) {
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetMode, 0, 0, "", 0}),
              std::string("S_MOD:0#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetMode, 0, 1, "", 0}),
              std::string("S_MOD:1#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::SetPwmFrequency, 0, 1, "", 0}),
              std::string("S_PWMP:1#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::EnableOutput, 0, 1, "", 0}),
              std::string("S_CTRL:1#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::Apply, 0, 0, "", 0}),
              std::string("S_ALL#"));
}

TEST(build_query_commands) {
    EXPECT_EQ(protocol::Build({protocol::CommandType::Query, 0, 0, "VER", 0}),
              std::string("S_VER:#"));
    EXPECT_EQ(protocol::Build({protocol::CommandType::Query, 0, 0, "ERRS", 0}),
              std::string("S_ERRS:#"));
    EXPECT_TRUE(protocol::IsQuery({protocol::CommandType::Query, 0, 0, "VER", 0}));
    EXPECT_FALSE(protocol::IsQuery({protocol::CommandType::SetChannel, 1, 0, "", 0}));
}

TEST(parse_status_value_basic) {
    EXPECT_EQ(protocol::ParseStatusValue("VER:2#", "VER").value(), 2);
    EXPECT_EQ(protocol::ParseStatusValue("MOD:0#", "MOD").value(), 0);
    EXPECT_EQ(protocol::ParseStatusValue("TEMP:45#", "TEMP").value(), 45);
    EXPECT_EQ(protocol::ParseStatusValue("VOLT:1200#", "VOLT").value(), 1200);
    EXPECT_EQ(protocol::ParseStatusValue("CUR:800#", "CUR").value(), 800);
}

TEST(parse_status_value_case_and_space) {
    EXPECT_EQ(protocol::ParseStatusValue("ok ver  :  7 #", "ver").value(), 7);
    EXPECT_EQ(protocol::ParseStatusValue("S_VER:7#", "VER").value(), 7);
    EXPECT_EQ(protocol::ParseStatusValue("VEr:9#", "VER").value(), 9);
}

TEST(parse_status_value_negative_and_aliases) {
    EXPECT_EQ(protocol::ParseStatusValue("TEMP:-3#", "TEMP").value(), -3);
    EXPECT_EQ(protocol::ParseStatusValue("CRV:512#", "CRV").value(), 512);
    EXPECT_EQ(protocol::ParseStatusValue("CUR:512#", "CUR").value(), 512);
}

TEST(parse_status_value_missing) {
    EXPECT_FALSE(protocol::ParseStatusValue("MOD:0#", "VER").has_value());
    EXPECT_FALSE(protocol::ParseStatusValue("", "VER").has_value());
    EXPECT_FALSE(protocol::ParseStatusValue("VER:#", "VER").has_value());
    EXPECT_FALSE(protocol::ParseStatusValue("VERVALUE", "VER").has_value());
}

TEST(configure_stable_sequence) {
    auto sequence = protocol::ConfigureStableSequence(1);
    EXPECT_EQ(sequence.size(), size_t(3));
    EXPECT_EQ(protocol::Build(sequence[0]), std::string("S_MOD:0#"));
    EXPECT_EQ(protocol::Build(sequence[1]), std::string("S_PWMP:1#"));
    EXPECT_EQ(protocol::Build(sequence[2]), std::string("S_CTRL:1#"));
}

TEST(apply_sequences) {
    auto restored = protocol::ApplySequence(true);
    EXPECT_EQ(restored.size(), size_t(3));
    EXPECT_EQ(protocol::Build(restored[0]), std::string("S_ALL#"));
    EXPECT_EQ(protocol::Build(restored[1]), std::string("S_MOD:0#"));
    EXPECT_EQ(protocol::Build(restored[2]), std::string("S_PWMP:1#"));
    EXPECT_EQ(restored[0].read_ms, 320);

    auto plain = protocol::ApplySequence(false);
    EXPECT_EQ(plain.size(), size_t(1));
    EXPECT_EQ(protocol::Build(plain[0]), std::string("S_ALL#"));
}

TEST(set_channel_sequences) {
    auto plain = protocol::SetChannelRawSequence(1, 153, false);
    EXPECT_EQ(plain.size(), size_t(2));
    EXPECT_EQ(protocol::Build(plain[0]), std::string("SPA153#"));
    EXPECT_EQ(protocol::Build(plain[1]), std::string("S_ALL#"));

    auto restored = protocol::SetChannelRawSequence(2, 0, true);
    EXPECT_EQ(restored.size(), size_t(7));
    EXPECT_EQ(protocol::Build(restored[0]), std::string("S_MOD:0#"));
    EXPECT_EQ(protocol::Build(restored[3]), std::string("SPB0#"));
    EXPECT_EQ(protocol::Build(restored[6]), std::string("S_PWMP:1#"));
}

TEST(set_channels_sequences) {
    std::vector<int> raws = {153, 76, 0, 255};
    auto sequence = protocol::SetChannelsRawSequence(raws, false);
    EXPECT_EQ(sequence.size(), size_t(5));
    EXPECT_EQ(protocol::Build(sequence[0]), std::string("SPA153#"));
    EXPECT_EQ(protocol::Build(sequence[1]), std::string("SPB76#"));
    EXPECT_EQ(protocol::Build(sequence[2]), std::string("SPC0#"));
    EXPECT_EQ(protocol::Build(sequence[3]), std::string("SPD255#"));
    EXPECT_EQ(protocol::Build(sequence[4]), std::string("S_ALL#"));
}

TEST(query_status_sequence) {
    auto sequence = protocol::QueryStatusSequence();
    EXPECT_EQ(sequence.size(), size_t(8));
    EXPECT_EQ(protocol::Build(sequence[0]), std::string("S_VER:#"));
    EXPECT_EQ(protocol::Build(sequence[3]), std::string("S_CTRL:#"));
    EXPECT_EQ(protocol::Build(sequence[7]), std::string("S_ERRS:#"));
    EXPECT_EQ(sequence[0].read_ms, 260);
}

TEST(initialize_device_sequence) {
    auto sequence = protocol::InitializeDeviceSequence();
    EXPECT_EQ(sequence.size(), size_t(10));
    EXPECT_EQ(protocol::Build(sequence[0]), std::string("S_MOD:0#"));
    EXPECT_EQ(protocol::Build(sequence[4]), std::string("SPB0#"));
    EXPECT_EQ(protocol::Build(sequence[5]), std::string("SPC0#"));
    EXPECT_EQ(protocol::Build(sequence[6]), std::string("SPD0#"));
    EXPECT_EQ(protocol::Build(sequence[7]), std::string("S_ALL#"));
    EXPECT_EQ(protocol::Build(sequence[9]), std::string("S_PWMP:1#"));
}
