import Struct, { ExtractType } from "typed-struct";
export const BROADCAST_MAC = new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]);

export function isBroadcastMac(mac_addr: Uint8Array) {
    return mac_addr.every(x => x === 0xFF);
};

export enum node_state_t {
    STATE_IDLE,
    STATE_DISABLED,
    STATE_BUZZER_ACTIVE,
    STATE_SHUTDOWN,
    STATE_SHOW_BATTERY,
    STATE_CONFIG
};

export const node_info_t = new Struct('node_info_t')
    .UInt8('node_type')
    .UInt8('battery_percent')
    .UInt8('color')
    .UInt8Array('rgb', 3)
    .UInt8('current_state')
    .UInt32LE('buzzer_active_remaining_ms')
    .compile();
export type node_info_t = ExtractType<typeof node_info_t>;

export const peer_data_t = new Struct('peer_data_t')
    .UInt8Array('mac_addr', 6)
    .UInt32LE('last_seen')
    .UInt32LE('last_sent_ping_us')
    .UInt16LE('latency_us')
    .Int8('rssi')
    .Struct('node_info', node_info_t)
    .compile();
export type peer_data_t = ExtractType<typeof peer_data_t>;


export const arr_peer_data_t = new Struct('arr_peer_data_t')
    .StructArray('peer_data_t', peer_data_t)
    .compile();
export type arr_peer_data_t = ExtractType<typeof arr_peer_data_t>;


export function uint16_t(value: number) {
    const buffer = new ArrayBuffer(2);
    new DataView(buffer).setUint16(0, value, true /* littleEndian */);
    return buffer;
}

export type DeviceInfo = {
    device: USBDevice,
    vendorInterface: USBInterface,
    endpointIn: USBEndpoint,
    endpointOut: USBEndpoint,
};

export type UnconnectedDeviceInfo = Partial<DeviceInfo> & { isConnected: false; };
export type ConnectedDeviceInfo = DeviceInfo & { isConnected: true; };

export const USB_CODES = {
    classes: {
        0x00: "Use class information in the Interface Descriptors",
        0x01: "Audio",
        0x02: "Communications and Communications Device Class (CDC) Control",
        0x03: "Human Interface Device (HID)",
        0x05: "Physical",
        0x06: "Image",
        0x07: "Printer",
        0x08: "Mass Storage (MSD)",
        0x09: "8Hub",
        0x0A: "CDC-Data",
        0x0B: "Smart Card",
        0x0D: "Content Security",
        0x0E: "Video",
        0x0F: "Personal Healthcare",
        0x10: "Audio/Video Devices",
        0x11: "Billboard Device Class",
        0xDC: "Diagnostic Device",
        // 0x0E:	"Wireless Controller",
        0xEF: "Miscellaneous",
        0xFE: "Application Specific",
        0xFF: "Vendor Specific"
    }
};

export const INTERFACE_CLASS_VENDOR = 0xFF; // Vendor

export const COLORS = {
    0: 'red',
    1: 'yellow',
    2: 'orange',
    3: 'green',
    4: 'teal',
    5: 'blue',
    6: 'magenta',
    7: 'white'
};