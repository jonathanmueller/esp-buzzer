import { Button, ButtonGroup, Divider } from "@nextui-org/react";
import { useCallback } from "react";
import { Power } from "react-bootstrap-icons";
import DeviceAction from "./DeviceAction";
import { DeviceInfo } from "./util";


type GameBarProps = {
    deviceInfo: DeviceInfo,
    handleError: (e: any) => void;
};

function GameBar(props: GameBarProps) {
    const { deviceInfo, handleError } = props;
    const sendCommandToAll = useCallback((data: number[]) =>
        deviceInfo.device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: 0x30,  // Command
            value: 0,
            index: 0
        }, new Uint8Array([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, ...data]))
            .then(result => {
                console.log("result ", result);
            })
            .catch(handleError),
        [deviceInfo]);

    return <div className="rounded-xl p-5 bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">
        <DeviceAction description="Ping Interval" endText="ms" deviceInfo={deviceInfo} handleError={handleError} />
        <Divider orientation="vertical" className="h-12" />
        {/* <span className="grow" /> */}
        <ButtonGroup>
            <Button color="default" variant="flat" onPress={() => sendCommandToAll([0x31])}>Alle deaktivieren</Button>
            <Divider orientation="vertical" />
            <Button color="default" variant="flat" onPress={() => sendCommandToAll([0x32])}>Alle aktivieren</Button>
        </ButtonGroup>
        <Divider orientation="vertical" />
        <span className="grow" />
        <Button color="danger" variant="flat" startContent={<Power />} onPress={() => sendCommandToAll([0x50])}>Alle ausschalten</Button>
    </div>;

}

export default GameBar;