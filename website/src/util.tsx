import Struct, { ExtractType, typed } from "typed-struct";
export const BROADCAST_MAC = new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]);

export const EXPECTED_DEVICE_VERSION = 0x11;

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
    .UInt8('version')
    .UInt8('node_type')
    .UInt8('battery_percent')
    .UInt32LE('battery_voltage')
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
    .Boolean8('valid_version')
    .Struct('node_info', node_info_t)
    .compile();
export type peer_data_t = ExtractType<typeof peer_data_t>;

export enum led_effect_t {
    EFFECT_NONE,            // Only increase brightness
    EFFECT_FLASH_WHITE,     // Flash with a bright white light
    EFFECT_FLASH_BASE_COLOR // Flash with the buzzer's base color
};

export const game_config_t = new Struct('game_config_t')
    .UInt16LE('buzzer_active_time')                 // [ms] Duration the buzzer is kept active (0: singular event, 65535: never reset)
    .UInt16LE('deactivation_time_after_buzzing')    // [ms] Duration the buzzer is deactivated after buzzer_active_time has passed (0: can press again immediately, 65535: keep disabled forever)
    .UInt8('buzz_effect', typed<led_effect_t>())    // The effect to play when pressing the buzzer
    .Boolean8('can_buzz_while_other_is_active')     // Whether or not we can buzz while another buzzer is active
    .Boolean8('must_release_before_pressing')       // Whether or not we have to release the buzzer before pressing to register
    .UInt16LE('crc')                                // CRC-16/GENIBUS of the game config (using esp_rom_crc16_be over all previous bytes)
    .compile();
export type game_config_t = ExtractType<typeof game_config_t>;


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
    cdcInterface: USBInterface,
    serialPort?: SerialPort,
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

export const INTERFACE_CLASS_CDC = 0x0A; // CDC-Data
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