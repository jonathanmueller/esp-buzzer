import { Buffer } from 'buffer';
import { arr_peer_data_t, command_t, game_config_t, peer_data_t, UUID_CHARACTERISTIC_EXEC_COMMAND, UUID_CHARACTERISTIC_PEER_LIST, UUID_CHARACTERISTIC_VERSION, UUID_SERVICE } from "../util";
import BuzzerDevice from "./BuzzerDevice";

export class BTBuzzerDevice implements BuzzerDevice {
    device: BluetoothDevice;
    deviceVersion: number = 0;

    versionCharacteristic?: BluetoothRemoteGATTCharacteristic;
    peerListCharacteristic?: BluetoothRemoteGATTCharacteristic;
    execCommandCharacteristic?: BluetoothRemoteGATTCharacteristic;
    peers?: peer_data_t[];
    readPeerListInterval?: number;

    constructor(device: BluetoothDevice) {
        this.device = device;
    }
    async connect(): Promise<void> {

        console.log("Connecting...");
        await this.device.gatt?.connect();

        // this.device.addEventListener("gattserverdisconnected", () => setDevice(undefined));

        let service = await this.device.gatt?.getPrimaryService(UUID_SERVICE);
        this.versionCharacteristic = await service?.getCharacteristic(UUID_CHARACTERISTIC_VERSION);
        this.peerListCharacteristic = await service?.getCharacteristic(UUID_CHARACTERISTIC_PEER_LIST);
        this.execCommandCharacteristic = await service?.getCharacteristic(UUID_CHARACTERISTIC_EXEC_COMMAND);

        this.deviceVersion = await this.readDeviceVersion();

        console.log("Connected");
    }

    getDeviceVersion(): number { return this.deviceVersion; }

    async getPeers(): Promise<peer_data_t[]> {

        if (!this.peerListCharacteristic) { return Promise.reject("No peer list characteristic"); }

        return await this.peerListCharacteristic.readValue().then(data => {
            console.log("Got updated peer list");
            const peers = new arr_peer_data_t(Buffer.from(data.buffer)).peer_data_t;
            return peers;
        });
    }

    isConnected(): boolean {
        return this.device.gatt?.connected ?? false;
    }
    async getPingInterval(): Promise<number> {
        return Promise.reject('Method not implemented.');
    }
    async setPingInterval(interval: number): Promise<void> {
        return Promise.reject('Method not implemented.');
    }
    async getGameConfig(): Promise<game_config_t> {
        return Promise.reject('Method not implemented.');
    }
    async setGameConfig(config: game_config_t): Promise<void> {
        return this.sendCommandToAll(command_t.COMMAND_SET_GAME_CONFIG, game_config_t.raw(config));
    }
    async sendCommandToAll(command: command_t, args?: number[]): Promise<void> {
        await this.execCommandCharacteristic?.writeValue(new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, command, ...args ?? []]));
    }

    async readDeviceVersion(): Promise<number> {
        if (!this.versionCharacteristic) { return Promise.reject("No version characteristic"); }

        let version = await this.versionCharacteristic.readValue().then(data => {
            const version = Buffer.from(data.buffer).readUInt8(0);
            console.log("Got device version: ", version);
            return version;
        });

        return version;
    }

    async sendCommand(peer: peer_data_t, command: command_t, args?: number[]): Promise<void> {
        await this.execCommandCharacteristic?.writeValue(new Uint8Array([...peer.mac_addr, command, ...args ?? []]));

    }

}

export default BTBuzzerDevice;