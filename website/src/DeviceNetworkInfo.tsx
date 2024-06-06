import { Button, Card, CardBody, CardFooter, CardHeader, Chip, Divider, Dropdown, DropdownItem, DropdownMenu, DropdownTrigger, Popover, PopoverContent, PopoverTrigger, Spinner, Switch, Tooltip } from '@nextui-org/react';
import { Buffer } from 'buffer';
import classNames from "classnames";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { ArrowClockwise, Command, InfoCircle, Option, Palette, Power, Reception0, Reception1, Reception2, Reception3, Reception4, Shift } from 'react-bootstrap-icons';
import { CirclePicker } from 'react-color';
import { DeviceInfo, arr_peer_data_t, isBroadcastMac, key_config_t, key_modifier_t, node_info_t, node_state_t, peer_data_t } from "./util";


export type DeviceNetworkInfoProps = {
    deviceInfo: DeviceInfo,
    handleError: (e: any) => void;
};

type PeerInfoProps = {
    deviceInfo: DeviceInfo,
    peer: peer_data_t,
    handleError: (e: any) => void;
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
    return colorMap[node_info.color as keyof typeof colorMap] ?? 'transparent';
}


function getReceptionIcon(rssi: number) {
    if (rssi === 0) return <Reception0 />;

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
    const { peer, deviceInfo, handleError } = props;
    const color = peer.valid_version ? colorFromPeerInfo(peer.node_info) : 'gray';
    const [showColorPicker, setShowColorPicker] = useState(false);
    const [showKeybindPicker, setShowKeybindPicker] = useState(false);

    const sendCommand = useCallback((data: number[]) =>
        deviceInfo.device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: 0x30,  // Command
            value: 0,
            index: 0
        }, new Uint8Array([...peer.mac_addr, ...data]))
            .then(result => {
                console.log("result ", result);
            })
            .catch(handleError),
        [deviceInfo, peer, handleError]);

    const buzz = useCallback(() => sendCommand([0x30]), [sendCommand]);
    const [active, _setActive] = useState(peer.node_info.current_state != 1);
    useEffect(() => { if (peer.node_info.current_state != 1) { _setActive(true); } }, [peer.node_info.current_state]);
    const setActive = useCallback((active: boolean) => { sendCommand([active ? 0x32 : 0x31]); _setActive(active); }, [sendCommand]);

    const menuActions = useMemo(() => ({
        "setColor": () => setTimeout(() => setShowColorPicker(true), 100),
        "setKeyConfig": () => setTimeout(() => setShowKeybindPicker(true), 0),
        "reset": () => sendCommand([0x40]),
        "shutdown": () => sendCommand([0x50])
    }), [sendCommand]);

    return <>
        <Card className={classNames("transition",
            peer.node_info.current_state == node_state_t.STATE_BUZZER_ACTIVE && "ring-4 ring-white scale-110 z-10",
            peer.node_info.current_state == node_state_t.STATE_DISABLED && "opacity-50"
        )} style={{ "--color": color } as React.CSSProperties}>
            <CardHeader className="flex gap-3">
                <Tooltip showArrow content={<span>
                    MAC: {Array.from(peer.mac_addr).map(x => x.toString(16).padStart(2, '0')).join(":")}<br />
                </span>}>
                    <InfoCircle className="text-foreground-500" />
                </Tooltip>
                <span className="flex-grow" />
                {!peer.valid_version && <>
                    <Chip color="danger" size='sm'>Falsche SW-Version</Chip>
                    <span className="flex-grow" />
                </>}
                {peer.valid_version && <Chip size="sm" variant='flat'>{peer.latency_us === 0 ? '\u2013 ' : `${(peer.latency_us / 1000).toFixed(1)}`}ms</Chip>}
                <Tooltip showArrow content={peer.rssi === 0 ? '-' : `${peer.rssi}dBm`}>{getReceptionIcon(peer.rssi)}</Tooltip>
                {peer.valid_version && <Tooltip showArrow content={peer.node_info.battery_percent === 0 ? '-' : `${peer.node_info.battery_percent}% (${(peer.node_info.battery_voltage / 1000).toFixed(2)}V)`}>{getBatteryIcon(peer.node_info.battery_percent)}</Tooltip>}
            </CardHeader>
            <Divider />

            <CardBody>
                <Button
                    isDisabled={!peer.valid_version}
                    className=" mx-auto m-5 w-40 h-auto rounded-full aspect-square shadow-md shadow-black ring-2 ring-white bg-[var(--color)]"
                    onPress={buzz} />
                {peer.valid_version && <div className="absolute bottom-0 right-0 me-2 mb-2 px-2 py-1 text-xs border rounded-md border-gray-600 text-gray-500">{renderKeyConfig(peer.node_info.key_config)}</div>}
            </CardBody>

            <Divider />
            <CardFooter className="flex flex-row flex-nowrap gap-3 items-center py-0">
                <Switch className="py-3" size="sm" defaultSelected isSelected={active} isDisabled={!peer.valid_version} onValueChange={setActive}>Aktiv</Switch>
                <Divider orientation="vertical" />


                <Dropdown className='dark text-foreground'>
                    <DropdownTrigger>
                        <Button variant="light" className="px-5" isDisabled={!peer.valid_version}>
                            Optionen
                        </Button>
                    </DropdownTrigger>
                    <DropdownMenu onAction={key => menuActions[key as keyof typeof menuActions]()} aria-label="Buzzer Optionen">
                        <DropdownItem key="setColor" startContent={<Palette />}>Farbe</DropdownItem>
                        <DropdownItem showDivider key="setKeyConfig" startContent={<Command />}>Keybind setzen</DropdownItem>
                        <DropdownItem key="reset" startContent={<ArrowClockwise />}>Reset</DropdownItem>
                        <DropdownItem key="shutdown" className="text-danger" color='danger' startContent={<Power />}>Ausschalten</DropdownItem>
                    </DropdownMenu>
                </Dropdown>
                <Popover className="dark text-foreground " placement="right" backdrop='transparent' isOpen={showColorPicker} onOpenChange={(open) => setShowColorPicker(open)}>
                    <PopoverTrigger><span></span></PopoverTrigger>
                    <PopoverContent className="p-5">
                        <CirclePicker color={color} onChange={v => sendCommand([0x20, 0xFF, v.rgb.r, v.rgb.g, v.rgb.b])} />
                    </PopoverContent>
                </Popover>
                <Popover className="dark text-foreground " placement="right" backdrop='transparent' isOpen={showKeybindPicker} onOpenChange={(open) => setShowKeybindPicker(open)}>
                    <PopoverTrigger><span></span></PopoverTrigger>
                    <PopoverContent className="p-5">
                        <KeybindPicker keybind={peer.node_info.key_config} onChange={v => sendCommand([0x22, v.modifiers, v.scan_code])} />
                    </PopoverContent>
                </Popover>
            </CardFooter>
        </Card>
    </>;
}


function renderKeyConfig(key_config: key_config_t) {
    const { modifiers, scan_code } = key_config;

    let char = "";
    if (scan_code >= 0x04 && scan_code <= 0x1d) {
        char = String.fromCharCode(scan_code - 0x04 + 'A'.charCodeAt(0));
    } else if (scan_code >= 0x1e && scan_code <= 0x27) {
        char = String.fromCharCode(((scan_code - 0x1d) % 10) + '0'.charCodeAt(0));
    } else {
        char = "";
    }

    return <div className="flex flex-row h-full items-center w-full justify-center" style={{ gap: ".35em" }}>
        {((modifiers & key_modifier_t.ALL_CTRL) !== 0) && `Ctrl`}
        {((modifiers & key_modifier_t.ALL_SHIFT) !== 0) && <Shift />}
        {((modifiers & key_modifier_t.ALL_ALT) !== 0) && <Option />}
        {((modifiers & key_modifier_t.ALL_GUI) !== 0) && <Command />}
        <div>{char}</div>
    </div >;
}

type KeybindPickerProps = {
    keybind: key_config_t,
    onChange: (config: key_config_t) => void;
};

function KeybindPicker(props: KeybindPickerProps) {
    const { keybind, onChange } = props;
    const keybindElement = useRef<HTMLDivElement>(null);

    const [newKeybind, setNewKeybind] = useState<key_config_t>(keybind);
    const [modifiersLocked, setModifiersLocked] = useState(false);
    const [mainCharPressed, setMainCharPressed] = useState(false);

    useEffect(() => {
        console.log("Focus...");
        keybindElement.current?.focus();
    }, []);


    const keyDownUp = useCallback((e: React.KeyboardEvent) => {
        e.preventDefault();
        if (e.repeat) { return; }

        let { modifiers, scan_code } = newKeybind;

        let currentModifiers = 0;
        if (e.getModifierState('Control')) currentModifiers |= key_modifier_t.LEFT_CTRL;
        if (e.getModifierState('Shift')) currentModifiers |= key_modifier_t.LEFT_SHIFT;
        if (e.getModifierState('Alt')) currentModifiers |= key_modifier_t.LEFT_ALT;
        if (e.getModifierState('Meta')) currentModifiers |= key_modifier_t.LEFT_GUI;

        if (!modifiersLocked) {
            modifiers = currentModifiers;
        }

        if (e.type == "keyup") {
            if (currentModifiers === 0 && !mainCharPressed) {
                setModifiersLocked(false);
            }
        }
        const mainButtonPressed = (new_scan_code: number) => {
            if (e.type == "keydown") {
                scan_code = new_scan_code;
                setModifiersLocked(true);
            }
            if (scan_code === newKeybind.scan_code) setMainCharPressed(e.type == "keydown");
        };

        if (e.key.toLowerCase().match(/^[a-z]$/)) {
            mainButtonPressed(0x04 + (e.key.toLowerCase().charCodeAt(0) - 'a'.charCodeAt(0)));
            // if (e.type == "keydown") {
            //     scan_code = 0x04 + (e.key.toLowerCase().charCodeAt(0) - 'a'.charCodeAt(0));
            //     setModifiersLocked(true);
            // }
            // if (scan_code === newKeybind.scan_code) setMainCharPressed(e.type == "keydown");
        } else if (e.code.match(/^(Digit|Numpad)/)) {
            mainButtonPressed(0x1d + ((e.code.replace(/^(Digit|Numpad)/, "").charCodeAt(0) - '0'.charCodeAt(0)) % 10));
            // if (e.type == "keydown") {
            //     scan_code = 0x1d + ((e.code.replace(/^(Digit|Numpad)/, "").charCodeAt(0) - '0'.charCodeAt(0)) % 10);
            //     setModifiersLocked(true);
            // }
            // if (scan_code === newKeybind.scan_code) setMainCharPressed(e.type == "keydown");
        } else if (e.type == "keydown") {
            scan_code = 0;
        }


        setNewKeybind({ modifiers, scan_code });
    }, [modifiersLocked, newKeybind]);


    return <div className="flex flex-row flex-nowrap">
        <div tabIndex={1} ref={keybindElement} onKeyDown={keyDownUp} onKeyUp={keyDownUp} className="w-24 px-2 py-1 rounded-md border-2 border-gray-600">{renderKeyConfig(newKeybind)}</div>
        <Button variant="bordered" className="ml-5 px-5" onClick={() => onChange(newKeybind)}>Speichern</Button>
    </div>;
}

export function DeviceNetworkInfo(props: DeviceNetworkInfoProps) {
    const { deviceInfo, handleError } = props;
    const { device } = deviceInfo;

    const [peers, setPeers] = useState<peer_data_t[]>([]);

    const fetchValue = useCallback(async () => {
        await device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: 0x20,
            value: 0,
            index: 0
        }, peer_data_t.baseSize * 20)
            .then(result => {
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
        const interval = setInterval(fetchValue, 500);
        return () => {
            clearInterval(interval);
        };
    }, [fetchValue]);

    return <div className="mt-5 flex gap-5">
        {peers.map((peer, i) => <PeerInfo key={i} deviceInfo={deviceInfo} peer={peer} handleError={handleError} />)}
        {peers.length == 0 && <Card className="p-5"><CardBody><Spinner size="lg" color="white" label="Suche..." /></CardBody></Card>}
    </div>;
}

export default DeviceNetworkInfo;
