/*
    device.c -- Interaction with Windows tap driver in a MinGW environment
    Copyright (C) 2002-2005 Ivo Timmermans,
                  2002-2013 Guus Sliepen <guus@tinc-vpn.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "../system.h"

#include <windows.h>
#include <winioctl.h>

#include "../conf.h"
#include "../device.h"
#include "../logger.h"
#include "../names.h"
#include "../net.h"
#include "../route.h"
#include "../utils.h"
#include "../xalloc.h"

#include "common.h"

int device_fd = -1;
static HANDLE device_handle = INVALID_HANDLE_VALUE;
static io_t device_read_io;
static OVERLAPPED device_read_overlapped;
static vpn_packet_t device_read_packet;
char *device = NULL;
char *iface = NULL;
static char *device_info = NULL;

extern char *myport;

static void device_issue_read() {
	device_read_overlapped.Offset = 0;
	device_read_overlapped.OffsetHigh = 0;

	int status;
	for (;;) {
		DWORD len;
		status = ReadFile(device_handle, (void *)device_read_packet.data, MTU, &len, &device_read_overlapped);
		if (!status) {
			if (GetLastError() != ERROR_IO_PENDING)
				logger(DEBUG_ALWAYS, LOG_ERR, "Error while reading from %s %s: %s", device_info,
					   device, strerror(errno));
			break;
		}

		device_read_packet.len = len;
		device_read_packet.priority = 0;
		route(myself, &device_read_packet);
	}
}

static void device_handle_read(void *data, int flags) {
	ResetEvent(device_read_overlapped.hEvent);

	DWORD len;
	if (!GetOverlappedResult(device_handle, &device_read_overlapped, &len, FALSE)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Error getting read result from %s %s: %s", device_info,
			   device, strerror(errno));
		return;
	}

	device_read_packet.len = len;
	device_read_packet.priority = 0;
	route(myself, &device_read_packet);
	device_issue_read();
}

static bool setup_device(void) {
	HKEY key, key2;
	int i;

	char regpath[1024];
	char adapterid[1024];
	char adaptername[1024];
	char tapname[1024];
	DWORD len;

	bool found = false;

	int err;

	get_config_string(lookup_config(config_tree, "Device"), &device);
	get_config_string(lookup_config(config_tree, "Interface"), &iface);

	/* Open registry and look for network adapters */

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &key)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Unable to read registry: %s", winerror(GetLastError()));
		return false;
	}

	for (i = 0; ; i++) {
		len = sizeof adapterid;
		if(RegEnumKeyEx(key, i, adapterid, &len, 0, 0, 0, NULL))
			break;

		/* Find out more about this adapter */

		snprintf(regpath, sizeof regpath, "%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, adapterid);

		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, regpath, 0, KEY_READ, &key2))
			continue;

		len = sizeof adaptername;
		err = RegQueryValueEx(key2, "Name", 0, 0, (LPBYTE)adaptername, &len);

		RegCloseKey(key2);

		if(err)
			continue;

		if(device) {
			if(!strcmp(device, adapterid)) {
				found = true;
				break;
			} else
				continue;
		}

		if(iface) {
			if(!strcmp(iface, adaptername)) {
				found = true;
				break;
			} else
				continue;
		}

		snprintf(tapname, sizeof tapname, USERMODEDEVICEDIR "%s" TAPSUFFIX, adapterid);
		device_handle = CreateFile(tapname, GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
		if(device_handle != INVALID_HANDLE_VALUE) {
			found = true;
			break;
		}
	}

	RegCloseKey(key);

	if(!found) {
		logger(DEBUG_ALWAYS, LOG_ERR, "No Windows tap device found!");
		return false;
	}

	if(!device)
		device = xstrdup(adapterid);

	if(!iface)
		iface = xstrdup(adaptername);

	/* Try to open the corresponding tap device */

	if(device_handle == INVALID_HANDLE_VALUE) {
		snprintf(tapname, sizeof tapname, USERMODEDEVICEDIR "%s" TAPSUFFIX, device);
		device_handle = CreateFile(tapname, GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
	}

	if(device_handle == INVALID_HANDLE_VALUE) {
		logger(DEBUG_ALWAYS, LOG_ERR, "%s (%s) is not a usable Windows tap device: %s", device, iface, winerror(GetLastError()));
		return false;
	}

	/* Get MAC address from tap device */

	if(!DeviceIoControl(device_handle, TAP_IOCTL_GET_MAC, mymac.x, sizeof mymac.x, mymac.x, sizeof mymac.x, &len, 0)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Could not get MAC address from Windows tap device %s (%s): %s", device, iface, winerror(GetLastError()));
		return false;
	}

	if(routing_mode == RMODE_ROUTER) {
		overwrite_mac = 1;
	}

	device_info = "Windows tap device";

	logger(DEBUG_ALWAYS, LOG_INFO, "%s (%s) is a %s", device, iface, device_info);

	return true;
}

static void enable_device(void) {
	logger(DEBUG_ALWAYS, LOG_INFO, "Enabling %s", device_info);

	ULONG status = 1;
	DWORD len;
	DeviceIoControl(device_handle, TAP_IOCTL_SET_MEDIA_STATUS, &status, sizeof status, &status, sizeof status, &len, NULL);

	io_add_event(&device_read_io, device_handle_read, NULL, CreateEvent(NULL, TRUE, FALSE, NULL));
	device_read_overlapped.hEvent = device_read_io.event;
	device_issue_read();
}

static void disable_device(void) {
	logger(DEBUG_ALWAYS, LOG_INFO, "Disabling %s", device_info);

	io_del(&device_read_io);
	CancelIo(device_handle);
	CloseHandle(device_read_overlapped.hEvent);

	ULONG status = 0;
	DWORD len;
	DeviceIoControl(device_handle, TAP_IOCTL_SET_MEDIA_STATUS, &status, sizeof status, &status, sizeof status, &len, NULL);
}

static void close_device(void) {
	CloseHandle(device_handle); device_handle = INVALID_HANDLE_VALUE;

	free(device); device = NULL;
	free(iface); iface = NULL;
	device_info = NULL;
}

static bool read_packet(vpn_packet_t *packet) {
	return false;
}

static bool write_packet(vpn_packet_t *packet) {
	DWORD outlen;
	OVERLAPPED overlapped = {0};

	logger(DEBUG_TRAFFIC, LOG_DEBUG, "Writing packet of %d bytes to %s",
			   packet->len, device_info);

	if(!WriteFile(device_handle, packet->data, packet->len, &outlen, &overlapped)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Error while writing to %s %s: %s", device_info, device, winerror(GetLastError()));
		return false;
	}

	return true;
}

const devops_t os_devops = {
	.setup = setup_device,
	.close = close_device,
	.read = read_packet,
	.write = write_packet,
	.enable = enable_device,
	.disable = disable_device,
};
