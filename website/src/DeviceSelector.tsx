import { Button, ButtonGroup } from "@nextui-org/react";
import { useLocalStorage } from "@uidotdev/usehooks";
import classNames from "classnames";
import { useEffect, useRef, useState } from "react";
import BTBuzzerDevice from "./adapters/BTBuzzerDevice";
import BuzzerDevice from "./adapters/BuzzerDevice";
import USBBuzzerDevice from "./adapters/USBBuzzerDevice";
import { UUID_SERVICE } from "./util";


type DeviceSelectorProps = {
    onSelect: (device?: BuzzerDevice) => void;
    onConnectionStateChanged: () => void;
    // deviceInfo: DeviceInfo,
    // handleError: (e: any) => void;
};


function DeviceSelector(props: DeviceSelectorProps) {
    const { onSelect, onConnectionStateChanged } = props;

    const [connectionType, setConnectionType] = useLocalStorage<"usb" | "bluetooth">("connectionType", "usb");
    const [usbDevice, setUSBDevice] = useState<USBDevice>();
    const usbDeviceRef = useRef(usbDevice);
    const [btDevice, setBTDevice] = useState<BluetoothDevice>();
    const [device, setDevice] = useState<BuzzerDevice>();


    useEffect(() => {
        if (!navigator.usb) {
            return;
        }

        (async () => {
            let devices = await navigator.usb.getDevices();
            if (devices && devices.length) {
                setUSBDevice(devices[0]);
            }
        })();
    }, []);

    useEffect(() => {
        if (!navigator.bluetooth?.getDevices) {
            return;
        }

        (async () => {
            let devices = await navigator.bluetooth.getDevices();
            if (devices && devices.length) {
                setBTDevice(devices[0]);
            }
        })();
    }, []);

    useEffect(() => {
        if (!navigator.usb) {
            return;
        }

        const onConnect = (e: USBConnectionEvent) => {
            if (usbDeviceRef.current === undefined) {
                setUSBDevice(e.device);
            }
        };
        const onDisconnect = (e: USBConnectionEvent) => {
            console.log("on disconnect", e.device === usbDeviceRef.current);
            if (e.device === usbDeviceRef.current) {
                setUSBDevice(undefined);
            }
        };
        navigator.usb.addEventListener("connect", onConnect);
        navigator.usb.addEventListener("disconnect", onDisconnect);

        return () => {
            navigator.usb.removeEventListener("connect", onConnect);
            navigator.usb.removeEventListener("disconnect", onDisconnect);
        };
    }, []);
    const onClick = async () => {
        if (connectionType === "usb") {
            if (!usbDevice) {
                setUSBDevice(await navigator.usb.requestDevice({ filters: [{ vendorId: 0xcafe }] }).catch(_ => undefined));
                // connectToDevice();
            } else {
                try {
                    await usbDevice.close();
                } catch (e) {
                } finally {
                    setUSBDevice(undefined);
                }
            }
        } else if (connectionType === "bluetooth") {
            if (!btDevice) {
                setBTDevice(await navigator.bluetooth.requestDevice({
                    filters: [
                        {
                            // name: "Buzzer Controller",
                            services: [UUID_SERVICE],
                        },
                    ],
                }).catch(_ => undefined));
            } else {
                setBTDevice(undefined);
            }
        }
    };

    useEffect(() => {
        if (connectionType === "usb") {
            if (usbDevice !== undefined) {
                const device = new USBBuzzerDevice(usbDevice!);
                setDevice(device);
            } else {
                setDevice(undefined);
            }
        }
    }, [connectionType, usbDevice]);

    useEffect(() => {
        if (connectionType === "bluetooth") {
            if (btDevice !== undefined) {
                const device = new BTBuzzerDevice(btDevice!);
                setDevice(device);
            } else {
                setDevice(undefined);
            }
        }
    }, [connectionType, btDevice]);

    useEffect(() => {
        (async () => {
            await device?.connect();
            console.log("On connection status changed");
            onConnectionStateChanged();
        })();
    }, [device, onConnectionStateChanged]);

    useEffect(() => {
        onSelect(device);
    }, [device, onSelect]);


    return <>
        <div className="flex flex-col">
            <ButtonGroup size="sm">
                <Button variant={connectionType == "usb" ? "solid" : "flat"} onClick={() => setConnectionType("usb")}>USB</Button>
                <Button variant={connectionType == "bluetooth" ? "solid" : "flat"} onClick={() => setConnectionType("bluetooth")}>Bluetooth</Button>
            </ButtonGroup>
            <div onClick={onClick} className={classNames("connect-icon h-[4rem] mb-[-0.5rem] self-center transition hover:scale-110", device?.isConnected() && "bg-green-600")} />
        </div>
    </>;
}

export default DeviceSelector;

