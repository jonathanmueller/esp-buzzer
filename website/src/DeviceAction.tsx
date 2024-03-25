import { Button } from "@nextui-org/button";
import { Input } from "@nextui-org/react";
import { useEffect, useState } from "react";
import { DeviceInfo, uint16_t } from "./util";


// const NodeInfo = new Struct('MyStructure') // give a name to the constructor
//   .Int8('foo')        // signed 8-bit integer field `foo`
//   .UInt16LE('bar')    // unsigned, little-endian 16-bit integer field `bar`
//   .compile();         // create a constructor for the structure, called last

type DeviceActionProps = {
    description?: string,
    endText?: string,
    deviceInfo: DeviceInfo,
    handleError: (e: any) => void;
};

function DeviceAction(props: DeviceActionProps) {
    const { description, deviceInfo: { device }, handleError, endText } = props;
    const [response, setResponse] = useState();
    const [currentValue, setCurrentValue] = useState(0);


    const fetchValue = async () => {
        console.log("fetching ping");
        await device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: 0x10,
            value: 0x10,
            index: 0
        }, 2)
            .then(result => {
                console.log("ping response", result);
                if (result.data) {
                    setCurrentValue(result.data.getUint16(0, true));
                }
            })
            .catch(props.handleError);
    };

    const updateValue = async (value: number) => {
        await device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: 0x10,
            value: 0x10,
            index: 0
        }, uint16_t(value))
            .then(result => {
                console.log("set ping interval response", result);
            })
            .catch(handleError);
    };


    useEffect(() => {
        fetchValue();
        return () => { };
    }, []);

    return <div className="flex w-full flex-wrap md:flex-nowrap gap-4 items-end">
        <Input
            type="number"
            color="default"
            label={description}
            labelPlacement="outside-left"
            value={currentValue.toString()}
            className="max-w-fit"
            onChange={e => setCurrentValue(e.target.valueAsNumber)}
            onKeyDown={e => { if (e.key === "Enter") { updateValue(currentValue); } }}
            endContent={endText && <div className="pointer-events-none flex items-center">
                <span className="text-default-400 text-small">{endText}</span>
            </div>}
        />
        <Button color="primary" onClick={() => updateValue(currentValue)}>Update</Button>
    </div>;
}
export default DeviceAction;
