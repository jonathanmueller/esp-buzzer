import { command_t, game_config_t, peer_data_t } from "../util";

export interface BuzzerDevice {
    connect(): Promise<void>;

    isConnected(): boolean;
    getDeviceVersion(): number;

    getPingInterval(): Promise<number>;
    setPingInterval(interval: number): Promise<void>;

    getGameConfig(): Promise<game_config_t>;
    setGameConfig(config: game_config_t): Promise<void>;

    getPeers(): Promise<peer_data_t[]>;

    sendCommand(peer: peer_data_t, command: command_t, args?: number[]): Promise<void>;
    sendCommandToAll(command: command_t, args?: number[]): Promise<void>;
};

export default BuzzerDevice;