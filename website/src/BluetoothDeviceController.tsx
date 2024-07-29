import { Button } from "@nextui-org/react";
import { Buffer } from 'buffer';
import { useCallback, useEffect, useState } from "react";
import DeviceNetworkInfo from "./DeviceNetworkInfo";
import { arr_peer_data_t, peer_data_t } from "./util";

interface BluetoothDeviceControllerProps {
    // onConnect: (device: BluetoothDevice) => void;
    handleError: (e: any) => void;
}

const UUID_SERVICE = "20d86bb5-f515-4671-8a88-32fddb20920c";
const UUID_CHARACTERISTIC_VERSION = "4d3c98dc-2970-496a-bc20-c1295abc9730";
const UUID_CHARACTERISTIC_EXEC_COMMAND = "d384392d-e53e-4c21-a598-f7bf8ccfcb66";
const UUID_CHARACTERISTIC_PEER_LIST = "f7551fb0-05c3-4dff-a944-4980f40779e1";

export const BluetoothDeviceController = (props: BluetoothDeviceControllerProps) => {
    const { handleError } = props;
    const [device, setDevice] = useState<BluetoothDevice>();

    const [peerListCharacteristic, setPeerListCharacteristic] = useState<BluetoothRemoteGATTCharacteristic>();
    const [execCommandCharacteristic, setExecCommandCharacteristic] = useState<BluetoothRemoteGATTCharacteristic>();
    const [peers, setPeers] = useState<peer_data_t[]>([]);
    const [readPeerListInterval, setReadPeerListInterval] = useState<number>();

    useEffect(() => {
        if (!navigator.bluetooth?.getDevices) {
            return;
        }

        (async () => {
            let devices = await navigator.bluetooth.getDevices();
            if (devices && devices.length) {
                setDevice(devices[0]);
            }
        })();
    }, []);

    const selectDevice = async () => {
        const device = await navigator.bluetooth.requestDevice({
            filters: [
                {
                    // name: "Buzzer Controller",
                    services: [UUID_SERVICE],
                },
            ],
        });
        setDevice(device);
    };

    useEffect(() => {
        (async () => {
            if (!device) { return; }

            console.log("Connecting...");
            await device.gatt?.connect();

            device.addEventListener("gattserverdisconnected", () => setDevice(undefined));

            let service = await device.gatt?.getPrimaryService(UUID_SERVICE);

            setPeerListCharacteristic(await service?.getCharacteristic(UUID_CHARACTERISTIC_PEER_LIST));
            setExecCommandCharacteristic(await service?.getCharacteristic(UUID_CHARACTERISTIC_EXEC_COMMAND));

            console.log("Connected");
        })();
    }, [device]);



    const readPeerList = useCallback(async () => {
        if (!peerListCharacteristic) { return; }

        peerListCharacteristic.readValue().then(data => {
            console.log("Got updated peer list");
            const peers = new arr_peer_data_t(Buffer.from(data.buffer)).peer_data_t;
            setPeers(peers);
        }).catch(e => {
            // console.log(e);
        });

    }, [peerListCharacteristic, setPeers]);

    useEffect(() => {
        if (!device || !peerListCharacteristic) { return; }

        readPeerList();

        console.log(peerListCharacteristic);

        peerListCharacteristic.startNotifications().then(_ => {
            device.oncharacteristicvaluechanged = _ => readPeerList();
        }).catch(e => {
            setReadPeerListInterval(setInterval(readPeerList, 1000));
        });


        return () => {
            clearInterval(readPeerListInterval);
            setReadPeerListInterval(undefined);
        };

    }, [device, readPeerList, peerListCharacteristic]);

    const sendCommand = useCallback(async (peer: peer_data_t, data: number[]) =>
        await execCommandCharacteristic?.writeValue(new Uint8Array([...peer.mac_addr, ...data]))
            .catch(handleError),
        [execCommandCharacteristic, handleError]);


    if (!navigator.bluetooth) {
        return <div className="rounded-xl p-5 bg-slate-500 dark:bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">Bluetooth not supported</div>;
    }

    return <>
        <div className="rounded-xl p-5 bg-slate-500 dark:bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">
            {!device && <Button onPress={selectDevice}>Connect</Button>}
            {device && <Button onPress={() => setDevice(undefined)}>Disconnect</Button>}
        </div >
        {device && peerListCharacteristic && <DeviceNetworkInfo peers={peers} sendCommand={sendCommand} handleError={handleError} />}
    </>;
};

export default BluetoothDeviceController;