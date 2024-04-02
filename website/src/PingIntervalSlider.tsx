import { Slider } from "@nextui-org/react";
import { useCallback, useEffect, useState } from "react";
import { DeviceInfo, uint16_t } from "./util";



type PingIntervalSliderProps = {
    deviceInfo: DeviceInfo,
    handleError: (e: any) => void;
};

function PingIntervalSlider(props: PingIntervalSliderProps) {
    const { deviceInfo: { device }, handleError } = props;
    const [currentValue, setCurrentValue] = useState(0);


    const fetchValue = useCallback(async () => {
        await device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: 0x10,
            value: 0x10,
            index: 0
        }, 2)
            .then(result => {
                if (result.data) {
                    setCurrentValue(result.data.getUint16(0, true));
                }
            })
            .catch(handleError);
    }, [device, handleError]);

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
    }, [fetchValue]);

    return <>
        {/* <Input
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
        <Button color="primary" onClick={() => updateValue(currentValue)}>Update</Button> */}
        <Slider
            value={currentValue}
            minValue={0}
            maxValue={10000}
            step={500}
            className="max-w-[250px]"
            // showTooltip={true}
            formatOptions={{ style: "unit", unit: "millisecond" }}
            // tooltipValueFormatOptions={{ maximumSignificantDigits: 1 }}
            onChange={value => setCurrentValue(value as number)}
            onChangeEnd={value => updateValue(value as number)}

            label="Ping Interval"
        />
    </>;
}
export default PingIntervalSlider;
