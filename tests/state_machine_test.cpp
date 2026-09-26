#include "test_harness.h"

#include "lamp/core/model.h"
#include "lamp/service/connection_state.h"

using namespace lamp;

TEST(state_machine_begin_connect) {
    ConnectionStateMachine machine;
    EXPECT_TRUE(machine.TryBeginConnect());
    EXPECT_EQ(machine.Current(), ConnectionState::Connecting);
}

TEST(state_machine_connect_success) {
    ConnectionStateMachine machine;
    machine.TryBeginConnect();
    EXPECT_TRUE(machine.TryConnected());
    EXPECT_EQ(machine.Current(), ConnectionState::Connected);
}

TEST(state_machine_failed_from_connecting) {
    ConnectionStateMachine machine;
    machine.TryBeginConnect();
    EXPECT_TRUE(machine.TryFailed());
    EXPECT_EQ(machine.Current(), ConnectionState::Failed);
}

TEST(state_machine_reconnect_cycle) {
    ConnectionStateMachine machine;
    machine.TryBeginConnect();
    machine.TryConnected();
    EXPECT_TRUE(machine.TryReconnect());
    EXPECT_EQ(machine.Current(), ConnectionState::Reconnecting);
    EXPECT_TRUE(machine.TryConnected());
    EXPECT_EQ(machine.Current(), ConnectionState::Connected);
}

TEST(state_machine_illegal_transitions) {
    ConnectionStateMachine machine;
    EXPECT_FALSE(machine.TryConnected());
    EXPECT_FALSE(machine.TryFailed());
    EXPECT_FALSE(machine.TryReconnect());

    machine.TryBeginConnect();
    EXPECT_FALSE(machine.TryBeginConnect());
    EXPECT_FALSE(machine.TryReconnect());

    machine.TryConnected();
    EXPECT_FALSE(machine.TryFailed());
    EXPECT_FALSE(machine.TryBeginConnect());
}

TEST(state_machine_disconnect_from_any_state) {
    ConnectionStateMachine machine;
    EXPECT_TRUE(machine.TryDisconnect());
    EXPECT_EQ(machine.Current(), ConnectionState::Disconnected);

    machine.TryBeginConnect();
    machine.TryConnected();
    EXPECT_TRUE(machine.TryDisconnect());
    EXPECT_EQ(machine.Current(), ConnectionState::Disconnected);
}

TEST(state_machine_retry_from_failed) {
    ConnectionStateMachine machine;
    machine.TryBeginConnect();
    machine.TryFailed();
    EXPECT_TRUE(machine.TryBeginConnect());
    EXPECT_EQ(machine.Current(), ConnectionState::Connecting);
}

TEST(state_machine_reset) {
    ConnectionStateMachine machine;
    machine.TryBeginConnect();
    machine.TryConnected();
    machine.Reset();
    EXPECT_EQ(machine.Current(), ConnectionState::Disconnected);
}
