import { Tooltip } from '@nextui-org/react';
import { Buffer } from 'buffer';
import classNames from "classnames";
import { useCallback, useEffect, useState } from "react";
import { Reception0, Reception1, Reception2, Reception3, Reception4 } from 'react-bootstrap-icons';
import { DeviceInfo, arr_peer_data_t, isBroadcastMac, node_info_t, node_state_t, peer_data_t } from "./util";


export type DeviceNetworkInfoProps = {
    deviceInfo: DeviceInfo,
    handleError: (e: any) => void;
};

type PeerInfoProps = {
    peer: peer_data_t;
};

enum color_t {
    COLOR_RED,
    COLOR_YELLOW,
    COLOR_ORANGE,
    COLOR_GREEN,
    COLOR_TEAL,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_WHITE,
    COLOR_RGB = 255
};

const colorMap = {
    [color_t.COLOR_RED]: "red",
    [color_t.COLOR_YELLOW]: "yellow",
    [color_t.COLOR_ORANGE]: "orange",
    [color_t.COLOR_GREEN]: "green",
    [color_t.COLOR_TEAL]: "teal",
    [color_t.COLOR_BLUE]: "blue",
    [color_t.COLOR_MAGENTA]: "magenta",
    [color_t.COLOR_WHITE]: "white",
};

function colorFromPeerInfo(node_info: node_info_t) {
    if (node_info.color === color_t.COLOR_RGB) {
        const rgb = node_info.rgb;
        return `rgb(${rgb[0]}, ${rgb[1]}, ${rgb[2]})`;
    }
    return colorMap[node_info.color as keyof typeof colorMap] ?? '';
}


function getReceptionIcon(rssi: number) {
    if (rssi > -70) return <Reception4 className="text-green-600" />;
    if (rssi > -85) return <Reception3 className="text-yellow-300" />;
    if (rssi > -100) return <Reception2 className="text-yellow-600" />;
    if (rssi > -110) return <Reception1 className="text-red-600" />;

    return <Reception0 className="text-red-600" />;
}

function getBatteryIcon(battery: number) {
    let className;

    if (battery > 60) className = "text-green-600";
    else if (battery > 20) className = "text-yellow-300";
    else className = "text-red-600";

    battery = Math.min(Math.max(battery, 0), 100);

    return <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" className={classNames("bi", "bi-battery-full", className)} viewBox="0 0 16 16">
        <path d={`M2 6h${battery / 10}v4H2z`} />
        <path d="M2 4a2 2 0 0 0-2 2v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2zm10 1a1 1 0 0 1 1 1v4a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1zm4 3a1.5 1.5 0 0 1-1.5 1.5v-3A1.5 1.5 0 0 1 16 8" />
    </svg>;
}

export function PeerInfo(props: PeerInfoProps) {
    const { peer } = props;
    const color = colorFromPeerInfo(peer.node_info);
    return <>
        <div className={classNames("border-1 p-4 rounded-xl", peer.node_info.current_state == node_state_t.STATE_BUZZER_ACTIVE && " ring-8 ring-yellow-300")} style={{ "--color": color } as React.CSSProperties}>
            <div className="flex gap-3">
                <span className="text-sm flex-grow">{peer.latency_us === 0 ? '-' : `${(peer.latency_us / 1000).toFixed(1)}ms`}</span>
                <Tooltip content={peer.rssi === 0 ? '-' : `${peer.rssi}dBm`}><span className="flex gap-2 items-baseline">{getReceptionIcon(peer.rssi)}</span></Tooltip>
                <Tooltip content={peer.node_info.battery_percent === 0 ? '-' : `${peer.node_info.battery_percent}%`}><span className="flex gap-2 items-baseline">{getBatteryIcon(peer.node_info.battery_percent)}</span></Tooltip>
            </div>
            <div className="peer-icon" />
            <div className="text-center text-foreground-400 text-sm">{Array.from(peer.mac_addr).map(x => x.toString(16).padStart(2, '0')).join(":")}<br /></div>
        </div>
        {/* <pre style={{ textAlign: 'left', lineBreak: 'anywhere', textWrap: 'wrap' }}>
            {JSON.stringify(peer)}
        </pre> */}
    </>;
}



export function DeviceNetworkInfo(props: DeviceNetworkInfoProps) {
    const { deviceInfo: { device }, handleError } = props;
    const [peers, setPeers] = useState<peer_data_t[]>([]);

    const fetchValue = useCallback(async () => {
        console.log("fetching network info");
        await device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: 32,
            value: 0,
            index: 0
        }, peer_data_t.baseSize * 20)
            .then(result => {
                console.log("network info response", result);
                if (result.data) {
                    const buf = Buffer.from(result.data.buffer);
                    const peers = new arr_peer_data_t(buf).peer_data_t;
                    setPeers(peers.filter(peer => !isBroadcastMac(peer.mac_addr)));
                } else {
                    setPeers([]);
                }
            })
            .catch(handleError);
    }, [device, handleError]);

    useEffect(() => {
        fetchValue();
        const interval = setInterval(fetchValue, 2000);
        return () => {
            clearInterval(interval);
        };
    }, [fetchValue]);

    return <div className="mt-5 flex gap-5">
        {peers.map((peer, i) => <PeerInfo key={i} peer={peer} />)}
    </div>;
}

export default DeviceNetworkInfo;
