import { Button } from "@nextui-org/react";
import { useCallback, useEffect, useState } from "react";

interface BluetoothDeviceControllerProps {
    // onConnect: (device: BluetoothDevice) => void;
}

const SERVICE_UUID = "20d86bb5-f515-4671-8a88-32fddb20920c";
const CHARACTERISTIC_UUID = "4d3c98dc-2970-496a-bc20-c1295abc9730";
const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();
export const BluetoothDeviceController = (props: BluetoothDeviceControllerProps) => {
    const [device, setDevice] = useState<BluetoothDevice>();

    const [characteristic, setCharacteristic] = useState<BluetoothRemoteGATTCharacteristic>();
    const [value, setValue] = useState<string>();

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
                    services: ["20d86bb5-f515-4671-8a88-32fddb20920c"],
                },
            ],
        });
        setDevice(device);
    };

    useEffect(() => {
        (async () => {
            if (!device) { return; }

            console.log(device);
            await device.gatt?.connect();

            let service = await device.gatt?.getPrimaryService(SERVICE_UUID);
            console.log(service);

            let characteristic = await service?.getCharacteristic(CHARACTERISTIC_UUID);
            setCharacteristic(characteristic);
        })();
    }, [device]);


    const readValue = useCallback((characteristic: BluetoothRemoteGATTCharacteristic) => {
        if (!characteristic) { setValue(undefined); return; }

        (async () => {
            setValue("Reading...");
            setValue(textDecoder.decode(await characteristic.readValue()));
        })();
    }, []);

    const update = useCallback(() => {
        if (!device || !characteristic) { return; }
        (async () => {
            setValue("Writing...");
            await characteristic.writeValue(textEncoder.encode("Test value " + Math.floor(Math.random() * 1000)));

            readValue(characteristic);
        })();
    }, [device, characteristic]);

    useEffect(() => {
        if (!device || !characteristic) { return; }

        readValue(characteristic);

        device.oncharacteristicvaluechanged = e => {
            console.log(e);
        };
    }, [device, characteristic, setValue]);



    if (!navigator.bluetooth) {
        return <div className="rounded-xl p-5 bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">Bluetooth not supported</div>;
    }

    return <div className="rounded-xl p-5 bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">
        {!device && <Button onPress={selectDevice}>Connect</Button>}
        {device &&
            <>
                <Button onPress={() => setDevice(undefined)}>Disconnect</Button>
                <Button className="ms-4" onPress={update}>Update with random value</Button>

                <div className="ms-4">Current Value: {value}</div>
            </>}
    </div >;
};

export default BluetoothDeviceController;