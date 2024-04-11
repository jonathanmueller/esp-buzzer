import Ansi from "ansi-to-react";
import { useEffect, useState } from "react";
import { DeviceNetworkInfoProps } from "./DeviceNetworkInfo";

const decoder = new TextDecoder();

type DeviceLogViewerProps = DeviceNetworkInfoProps & {};
function DeviceLogViewer(props: DeviceLogViewerProps) {
    const { deviceInfo, handleError } = props;
    const { device } = deviceInfo;

    const [log, setLog] = useState<ArrayBuffer>(new Uint8Array());

    useEffect(() => {

        const transferIn = async () => {
            await device
                .transferIn(1, 512)
                .then((result: USBInTransferResult) => {
                    if (result.status === "ok") {
                        setLog(l => {
                            let tmp = new Uint8Array(l.byteLength + result.data!.byteLength);
                            tmp.set(new Uint8Array(l), 0);
                            tmp.set(new Uint8Array(result.data!.buffer), l.byteLength);
                            return tmp.buffer;
                        });
                    }
                    // console.log("interrupt in", decoder.decode(result.data?.buffer));
                })
                .catch(async e => {
                    handleError(e);
                    await new Promise((res, _rej) => setTimeout(res, 1000));
                })
                .then(transferIn);
        };

        transferIn();

    }, [device, handleError]);

    return <pre className="text-small">
        <Ansi>
            {decoder.decode(log)}
        </Ansi>
    </pre>;
}

export default DeviceLogViewer;