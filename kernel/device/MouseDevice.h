/*
	This file is part of nusaOS.

	Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#pragma once

#include "kernel/interrupt/IRQHandler.h"
#include "kernel/device/CharacterDevice.h"
#include "kernel/kstd/circular_queue.hpp"
#include "kernel/api/hid.h"

#define I8042_BUFFER 0x60u
#define I8042_STATUS 0x64u

#define I8042_ACK 0xFAu
#define I8042_BUFFER_FULL 0x01u
#define I8042_WHICH_BUFFER 0x20u
#define I8042_MOUSE_BUFFER 0x20u
#define I8042_KEYBOARD_BUFFER 0x00u

#define MOUSE_SET_RESOLUTION 0xE8u
#define MOUSE_STATUS_REQUEST 0xE9u
#define MOUSE_REQUEST_SINGLE_PACKET 0xEBu
#define MOUSE_GET_DEVICE_ID 0xF2u
#define MOUSE_SET_SAMPLE_RATE 0xF3u
#define MOUSE_ENABLE_PACKET_STREAMING 0xF4u
#define MOUSE_DISABLE_PACKET_STREAMING 0xF5u
#define MOUSE_SET_DEFAULTS 0xF6u
#define MOUSE_RESEND 0xFEu
#define MOUSE_RESET 0xFFu
#define MOUSE_INTELLIMOUSE_ID 0x03u
#define MOUSE_INTELLIMOUSE_EXPLORER_ID 0x04u

class MouseDevice: public CharacterDevice, public IRQHandler {
public:
	static MouseDevice* inst();

	MouseDevice();

	ssize_t read(FileDescriptor& fd, size_t offset, SafePointer<uint8_t> buffer, size_t count) override;
	ssize_t write(FileDescriptor& fd, size_t offset, SafePointer<uint8_t> buffer, size_t count) override;
	bool can_read(const FileDescriptor& fd) override;
	bool can_write(const FileDescriptor& fd) override;

	void handle_irq(IRQRegisters* regs) override;
	void handle_byte(uint8_t byte);

private:
	void handle_packet();
	void handle_vmware_bytes();

	static MouseDevice* instance;
	bool present = false;
	bool has_scroll_wheel = false;
	uint8_t packet_data[4];
	uint8_t packet_state = 0;
	kstd::circular_queue<MouseEvent> event_buffer;
};