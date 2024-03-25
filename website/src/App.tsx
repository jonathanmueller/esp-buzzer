import classNames from 'classnames';
import React, { useCallback, useEffect, useMemo, useState } from 'react';
import './App.css';
import DeviceAction from './DeviceAction';
import DeviceNetworkInfo from './DeviceNetworkInfo';
import { ConnectedDeviceInfo, INTERFACE_CLASS_VENDOR, USB_CODES, UnconnectedDeviceInfo } from './util';


function getDeviceInfo(device?: USBDevice): UnconnectedDeviceInfo | ConnectedDeviceInfo {
  const vendorInterface = device?.configuration?.interfaces.filter(itf => itf.alternate.interfaceClass === INTERFACE_CLASS_VENDOR)[0];
  if (device && vendorInterface?.claimed) {
    return {
      device,
      vendorInterface,
      isConnected: true,
      endpointIn: vendorInterface?.alternate.endpoints.filter(endpoint => endpoint.direction === "in")[0],
      endpointOut: vendorInterface?.alternate.endpoints.filter(endpoint => endpoint.direction === "out")[0],
    };
  }

  return {
    device,
    vendorInterface,
    isConnected: false,
    endpointIn: vendorInterface?.alternate.endpoints.filter(endpoint => endpoint.direction === "in")[0],
    endpointOut: vendorInterface?.alternate.endpoints.filter(endpoint => endpoint.direction === "out")[0],
  };
}

function App() {
  const [error, setError] = useState<Error>();
  const [device, setDevice] = useState<USBDevice>();

  const [refreshDevice, setRefreshDevice] = useState({});

  const deviceInfo = useMemo(() => getDeviceInfo(device), [device, device?.opened, refreshDevice]);

  const handleError = useCallback((e: Error) => {
    if (e.name === "AbortError") { /* Ignore */ return; }

    // if (e.name === "NetworkError" && e.message.indexOf("Unable to claim interface") !== -1) {
    //   e = new Error("Device is already connected in another process");
    // }
    console.error(e);

    setError(e);
  }, [setError]);

  useEffect(() => {
    if (device) { return; }

    (async () => {
      let devices = await navigator.usb.getDevices();
      if (devices && devices.length) {
        setDevice(devices[0]);
        // connectToDevice();
      }
    })();
  }, []);

  useEffect(() => {
    const onConnect = (e: USBConnectionEvent) => {
      if (device === null) {
        setDevice(e.device);
        // connectToDevice();
      }
    };
    navigator.usb.addEventListener("connect", onConnect);
    return () => navigator.usb.removeEventListener("connect", onConnect);
  }, []);

  const onClick = async () => {
    if (!device) {
      setDevice(await navigator.usb.requestDevice({ filters: [{ vendorId: 0xcafe }] }).catch(e => undefined));
      // connectToDevice();
    } else {
      try {
        await device.close();
      } catch (e) {
      } finally {
        setDevice(undefined);
      }
    }
  };

  useEffect(() => {
    const connectToDevice = async (device?: USBDevice) => {
      if (device === undefined) {
        return;
      }

      try {
        setError(undefined);

        try {
          await device.open();
        } catch (e) {
          try { await device.close(); } catch (e) { }
          await new Promise((res, _) => setTimeout(res, 100));
          await device.open();
        }

        // await device.selectConfiguration(1);
        console.log("Claiming ", deviceInfo.vendorInterface?.interfaceNumber, deviceInfo);
        await device.claimInterface(deviceInfo.vendorInterface?.interfaceNumber ?? 0);

        console.log(device);

        // await device.transferOut(endpointIn.endpointNumber, encoder.encode("Dies ist ein Teststring"));

        device.transferIn(deviceInfo.endpointOut?.endpointNumber ?? 0, 10).then((result) => {
          const decoder = new TextDecoder();
          console.log("Received: " + decoder.decode(result.data));
        })
          .catch(handleError);

        // device.controlTransferIn(endpointIn.endpointNumber, 10).then(result => {
        //   const decoder = new TextDecoder();
        //   console.log("Received status: " + result.status + ", data: " + decoder.decode(result.data));
        // })
        // .catch(e => {
        //   if (e.name === "AbortError") { /* Ignore */ }
        //   else { throw e; }
        // })
        // .catch(e => setError(e));

        setRefreshDevice({});

      } catch (e: any) {
        handleError(e);
      }
    };
    connectToDevice(device);

    return () => {
      device?.close().catch(_ => { });
    };
  }, [device]);

  // .App-header {
  //   background-color: #282c34;
  //   min-height: 100vh;
  //   display: flex;
  //   flex-direction: column;
  //   align-items: center;
  //   justify-content: center;
  //   font-size: calc(10px + 2vmin);
  //   color: white;
  // }
  return (
    <div className="bg-gray-900">
      <div className="mx-auto container flex flex-col min-h-screen">
        <div onClick={onClick} className={classNames("connect-icon self-center", deviceInfo.isConnected && "bg-green-600")} />
        <div className="text-red-600 font-bold text-center">{error?.message ?? '\u00a0'}</div>
        {device && <div className="hidden">
          {device.configurations.map((config, i, configs) => <React.Fragment key={i}>
            {configs.length > 1 && <h1>{config.configurationName ?? "Configuration"} ({config.configurationValue})</h1>}
            {config.interfaces.map((itf, i) => <React.Fragment key={i}>
              <h2 className={classNames(itf.claimed && "text-amber-300")}>Interface {itf.interfaceNumber}: {USB_CODES.classes[itf.alternate.interfaceClass as keyof typeof USB_CODES.classes] ?? ("Class " + itf.alternate.interfaceClass)}</h2>
              {itf.alternates.map((alt, i, alts) => <React.Fragment key={i}>
                {alts.length > 1 && <h3>Alternate {alt.alternateSetting} ({USB_CODES.classes[alt.interfaceClass as keyof typeof USB_CODES.classes] ?? ("Class " + alt.interfaceClass)}, Subclass {alt.interfaceSubclass}, Protocol {alt.interfaceProtocol})</h3>}
                {alt.endpoints.map((endpoint, i) => <React.Fragment key={i}>
                  <h4>Endpoint {endpoint.endpointNumber} ({endpoint.direction}, {endpoint.type}, packet size {endpoint.packetSize})</h4>
                </React.Fragment>)}
              </React.Fragment>)}
            </React.Fragment>)}
          </React.Fragment>)}
        </div>}


        {device && deviceInfo.isConnected && <div>
          {/* <button onClick={() => setRefreshDevice({})}>Refresh Device Info</button> */}
          <DeviceAction description="Ping Interval" endText="ms" deviceInfo={deviceInfo} handleError={handleError} />
          {/* <button onClick={() => {device.controlOut}} /> */}

          <DeviceNetworkInfo deviceInfo={deviceInfo} handleError={handleError} />

        </div>}
      </div>
    </div>
  );
}

export default App;
