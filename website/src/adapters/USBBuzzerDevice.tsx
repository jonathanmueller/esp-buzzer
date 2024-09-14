import { Buffer } from "buffer";
import { arr_peer_data_t, command_t, game_config_t, INTERFACE_CLASS_CDC, INTERFACE_CLASS_VENDOR, peer_data_t, PEER_DATA_TABLE_ENTRIES, uint16_t, usb_request_t } from "../util";
import BuzzerDevice from "./BuzzerDevice";

export class USBBuzzerDevice implements BuzzerDevice {
    device: USBDevice;
    deviceVersion: number = 0;

    vendorInterface?: USBInterface;
    cdcInterface?: USBInterface;
    endpointOut?: USBEndpoint;
    endpointIn?: USBEndpoint;


    constructor(device: USBDevice) {
        this.device = device;
    }

    async connect(): Promise<void> {
        try {
            await this.device.open();
        } catch (e) {
            try { await this.device.close(); } catch (e) { }
            await new Promise((res, _) => setTimeout(res, 100));
            await this.device.open();
        }

        if (!this.device.configuration) {
            await this.device.selectConfiguration(this.device.configurations[0].configurationValue);
        }

        this.vendorInterface = this.device.configuration?.interfaces.filter(itf => itf.alternate.interfaceClass === INTERFACE_CLASS_VENDOR)[0];
        this.cdcInterface = this.device.configuration?.interfaces.filter(itf => itf.alternate.interfaceClass === INTERFACE_CLASS_CDC)[0];

        if (this.vendorInterface) {
            // await device.selectConfiguration(1);
            console.log("Claiming interface", this.vendorInterface.interfaceNumber);
            await this.device.claimInterface(this.vendorInterface.interfaceNumber);
        }
        this.endpointIn = this.vendorInterface?.alternate.endpoints.filter(endpoint => endpoint.direction === "in")[0];
        this.endpointOut = this.vendorInterface?.alternate.endpoints.filter(endpoint => endpoint.direction === "out")[0];

        this.deviceVersion = await this.readDeviceVersion();
    }

    getDeviceVersion(): number { return this.deviceVersion; }

    isConnected(): boolean {
        return (this.device && this.vendorInterface && this.cdcInterface && this.vendorInterface?.claimed) ?? false;
    }


    async readDeviceVersion(): Promise<number> {
        const versionInfo = await this.device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_VERSION,
            value: 0x00,
            index: 0
        }, 1);

        const version = versionInfo.data?.getUint8(0);

        if (version === undefined) {
            return Promise.reject("Could not retrieve device version");
        }

        return version;
    }

    async getPingInterval(): Promise<number> {
        return await this.device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_CONFIG,
            value: command_t.COMMAND_SET_PING_INTERVAL,
            index: 0
        }, 2)
            .then(result => {
                if (result.data) {
                    return result.data.getUint16(0, true);
                }
                throw new Error("Could not fetch ping interval");
            });
    }
    async setPingInterval(value: number): Promise<void> {
        await this.device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_CONFIG,
            value: command_t.COMMAND_SET_PING_INTERVAL,
            index: 0
        }, uint16_t(value))
            .then(result => {
                console.log("set ping interval response", result);
            });
    };

    async getGameConfig(): Promise<game_config_t> {
        return await this.device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_CONFIG,
            value: command_t.COMMAND_SET_GAME_CONFIG,
            index: 0
        }, game_config_t.baseSize)
            .then(result => {
                if (result.data) {
                    const buf = Buffer.from(result.data.buffer);
                    const gameConfig = new game_config_t(buf, true);
                    return gameConfig;
                }
                throw new Error("Could not fetch game config");
            });
    }

    async setGameConfig(config: game_config_t): Promise<void> {
        await this.device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_CONFIG,
            value: command_t.COMMAND_SET_GAME_CONFIG,
            index: 0
        }, game_config_t.raw(config));
        // .then(result => {
        // console.log("sent game config: ", result);
        // });
    }

    async sendCommand(peer: peer_data_t, command: command_t, args?: number[]): Promise<void> {
        await this.device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_SEND_COMMAND,  // Command
            value: 0,
            index: 0
        }, new Uint8Array([...peer.mac_addr, command, ...args ?? []]))
            .then(result => {
                console.log("result ", result);
            });
    }

    async sendCommandToAll(command: command_t, args?: number[]): Promise<void> {
        await this.device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_SEND_COMMAND,  // Command
            value: 0,
            index: 0
        }, new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, command, ...args ?? []]))
            .then(result => {
                console.log("result ", result);
            });
    }

    async getPeers(): Promise<peer_data_t[]> {
        return await this.device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: usb_request_t.USB_REQUEST_VENDOR_DEVICE_NETWORK_INFO,
            value: 0,
            index: 0
        }, peer_data_t.baseSize * PEER_DATA_TABLE_ENTRIES)
            .then(result => {
                if (result.data) {
                    const buf = Buffer.from(result.data.buffer);
                    const peers = new arr_peer_data_t(buf).peer_data_t;
                    return peers;
                } else {
                    return [];
                }
            });
    }
}

export default USBBuzzerDevice;