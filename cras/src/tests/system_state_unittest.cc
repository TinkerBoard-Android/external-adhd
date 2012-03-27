// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <gtest/gtest.h>

extern "C" {
#include "cras_system_state.h"
#include "cras_types.h"
}

namespace {
size_t volume_changed_called;
size_t volume_changed_value;
void *volume_changed_arg_value;
size_t mute_changed_called;
size_t mute_changed_value;
void *mute_changed_arg_value;

static void volume_changed(size_t volume, void *arg) {
  volume_changed_called++;
  volume_changed_value = volume;
  volume_changed_arg_value = arg;
}

static void mute_changed(int mute, void *arg) {
  mute_changed_called++;
  mute_changed_value = mute;
  mute_changed_arg_value = arg;
}

TEST(SystemStateSuite, DefaultVolume) {
  cras_system_state_init();
  EXPECT_EQ(100, cras_system_get_volume());
}

TEST(SystemStateSuite, SetVolume) {
  cras_system_state_init();
  cras_system_set_volume(0);
  EXPECT_EQ(0, cras_system_get_volume());
  cras_system_set_volume(50);
  EXPECT_EQ(50, cras_system_get_volume());
  cras_system_set_volume(CRAS_MAX_SYSTEM_VOLUME);
  EXPECT_EQ(CRAS_MAX_SYSTEM_VOLUME, cras_system_get_volume());
  cras_system_set_volume(CRAS_MAX_SYSTEM_VOLUME + 1);
  EXPECT_EQ(CRAS_MAX_SYSTEM_VOLUME, cras_system_get_volume());
}

TEST(SystemStateSuite, VolumeChangedCallback) {
  void * const fake_user_arg = (void *)1;
  const size_t fake_volume = 55;
  const size_t fake_volume_2 = 44;

  cras_system_state_init();
  cras_system_register_volume_changed_cb(volume_changed, fake_user_arg);
  volume_changed_called = 0;
  cras_system_set_volume(fake_volume);
  EXPECT_EQ(fake_volume, cras_system_get_volume());
  EXPECT_EQ(1, volume_changed_called);
  EXPECT_EQ(fake_volume, volume_changed_value);
  EXPECT_EQ(fake_user_arg, volume_changed_arg_value);

  cras_system_register_volume_changed_cb(NULL, NULL);
  volume_changed_called = 0;
  cras_system_set_volume(fake_volume_2);
  EXPECT_EQ(fake_volume_2, cras_system_get_volume());
  EXPECT_EQ(0, volume_changed_called);
}

TEST(SystemStateSuite, SetMute) {
  cras_system_state_init();
  EXPECT_EQ(0, cras_system_get_mute());
  cras_system_set_mute(0);
  EXPECT_EQ(0, cras_system_get_mute());
  cras_system_set_mute(1);
  EXPECT_EQ(1, cras_system_get_mute());
  cras_system_set_mute(22);
  EXPECT_EQ(1, cras_system_get_mute());
}

TEST(SystemStateSuite, MuteChangedCallback) {
  void * const fake_user_arg = (void *)1;

  cras_system_state_init();
  cras_system_register_volume_changed_cb(volume_changed, fake_user_arg);
  cras_system_register_mute_changed_cb(mute_changed, fake_user_arg);
  mute_changed_called = 0;
  cras_system_set_mute(1);
  EXPECT_EQ(1, cras_system_get_mute());
  EXPECT_EQ(1, mute_changed_called);
  EXPECT_EQ(1, mute_changed_value);
  EXPECT_EQ(fake_user_arg, mute_changed_arg_value);
  EXPECT_EQ(0, volume_changed_called);

  cras_system_register_mute_changed_cb(NULL, NULL);
  mute_changed_called = 0;
  cras_system_set_mute(0);
  EXPECT_EQ(0, cras_system_get_mute());
  EXPECT_EQ(0, mute_changed_called);
}

}  //  namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}