import { Card, CardBody, Spinner } from '@nextui-org/react';
import { useCallback, useEffect, useMemo, useState } from "react";
import BuzzerDevice from './adapters/BuzzerDevice';
import PeerInfo from './PeerInfo';
import { EXPECTED_DEVICE_VERSION, isBroadcastMac, isZeroMac, node_type_t, peer_data_t } from "./util";



export type DevicePeerListProps = {
    device: BuzzerDevice,
    handleError: (e: any) => void;
};
export function DevicePeerList(props: DevicePeerListProps) {
    const { device, handleError } = props;

    const deviceVersion = device.getDeviceVersion();

    const deviceError = useMemo(() => {
        if (deviceVersion !== EXPECTED_DEVICE_VERSION) {
            return `Unbekannte Geräteversion ${deviceVersion} (erwartet: ${EXPECTED_DEVICE_VERSION})`;
        }

        return undefined;
    }, [deviceVersion]);


    const [peers, setPeers] = useState<peer_data_t[]>([]);

    useEffect(() => setPeers([]), [device]);

    const fetchValue = useCallback(() => {
        device.getPeers()
            .then(setPeers)
            .catch(handleError);
    }, [device, handleError]);

    useEffect(() => {
        fetchValue();
        const interval = setInterval(fetchValue, 500);
        return () => {
            clearInterval(interval);
        };
    }, [fetchValue]);

    const sendCommand = device.sendCommand.bind(device);

    const filteredPeers = useMemo(() => peers.filter(peer =>
        !isBroadcastMac(peer.mac_addr) &&
        !isZeroMac(peer.mac_addr) &&
        (!peer.valid_version || peer.node_info.node_type != node_type_t.NODE_TYPE_CONTROLLER) /* Filter out controllers */
    ), [peers]);

    if (deviceError) {
        return <div className="text-red-600 font-bold text-center my-5">{deviceError}</div>;
    }

    return <div className="my-5 flex flex-wrap justify-center gap-5">
        {filteredPeers.map((peer, i) => <PeerInfo key={i} sendCommand={sendCommand} peer={peer} handleError={handleError} />)}
        {filteredPeers.length == 0 && <Card className="p-5"><CardBody><Spinner size="lg" color="white" label="Suche..." /></CardBody></Card>}
    </div>;
}

export default DevicePeerList;
