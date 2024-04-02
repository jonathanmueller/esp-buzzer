import { Divider } from '@nextui-org/react';
import classNames from 'classnames';
import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import './App.css';
import DeviceNetworkInfo from './DeviceNetworkInfo';
import GameBar from './GameBar';
import { ConnectedDeviceInfo, EXPECTED_DEVICE_VERSION, INTERFACE_CLASS_VENDOR, USB_CODES, UnconnectedDeviceInfo } from './util';


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
  const deviceRef = useRef(device);

  const [deviceVersion, setDeviceVersion] = useState<number>();

  const [refreshDevice, setRefreshDevice] = useState({});

  // eslint-disable-next-line react-hooks/exhaustive-deps
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
    (async () => {
      let devices = await navigator.usb.getDevices();
      if (devices && devices.length) {
        setDevice(devices[0]);
      }
    })();
  }, []);



  useEffect(() => {
    const onConnect = (e: USBConnectionEvent) => {
      if (deviceRef.current === undefined) {
        setDevice(e.device);
      }
    };
    const onDisconnect = (e: USBConnectionEvent) => {
      console.log("on disconnect", e.device === deviceRef.current);
      if (e.device === deviceRef.current) {
        setDevice(undefined);
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
    if (!device) {
      setDevice(await navigator.usb.requestDevice({ filters: [{ vendorId: 0xcafe }] }).catch(_ => undefined));
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


  const connectToDevice = useCallback(async (device?: USBDevice) => {
    setError(undefined);

    if (device === undefined) {
      return;
    }

    try {
      try {
        await device.open();
      } catch (e) {
        try { await device.close(); } catch (e) { }
        await new Promise((res, _) => setTimeout(res, 100));
        await device.open();
      }

      const deviceInfo = getDeviceInfo(device);

      // await device.selectConfiguration(1);
      console.log("Claiming interface ", deviceInfo.vendorInterface?.interfaceNumber, deviceInfo);
      await device.claimInterface(deviceInfo.vendorInterface?.interfaceNumber ?? 0);


      const versionInfo = await device.controlTransferIn({
        requestType: "vendor",
        recipient: "device",
        request: 0x00,
        value: 0x00,
        index: 0
      }, 1);

      const version = versionInfo.data?.getUint8(0);
      setDeviceVersion(version);

      setRefreshDevice({});

    } catch (e: any) {
      handleError(e);
    }
  }, [handleError]);

  useEffect(() => {
    deviceRef.current = device;
    setDeviceVersion(undefined);
    connectToDevice(device);

    return () => {
      device?.close().catch(_ => { });
      setDeviceVersion(undefined);
    };
  }, [device, connectToDevice]);

  const deviceError = useMemo(() => {
    if (deviceVersion && (deviceVersion !== EXPECTED_DEVICE_VERSION)) {
      return `Unbekannte Geräteversion ${deviceVersion} (erwartet: ${EXPECTED_DEVICE_VERSION})`;
    }

    return undefined;
  }, [deviceVersion]);

  return (
    <div className='bg-gray-800 '>
      <div className="mx-auto container flex flex-col min-h-screen">
        <div onClick={onClick} className={classNames("connect-icon self-center transition hover:scale-110", deviceError && "bg-red-600", !deviceError && deviceInfo.isConnected && "bg-green-600")} />
        {deviceError && <div className="text-red-600 font-bold text-center my-5">{deviceError}</div>}
        <div className="text-red-600 font-bold text-center my-5">{error?.message ?? '\u00a0'}</div>
        <Divider className="my-5" />
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


        {device && deviceInfo.isConnected && !deviceError && <div>
          <GameBar deviceInfo={deviceInfo} handleError={handleError} />

          <DeviceNetworkInfo deviceInfo={deviceInfo} handleError={handleError} />

        </div>}
      </div>
    </div>
  );
}

export default App;
