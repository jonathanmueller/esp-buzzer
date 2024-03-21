import classNames from 'classnames';
import React, { useEffect, useMemo, useState } from 'react';
import './App.css';

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const USB_CODES = {
  classes: {
    0x00:	"Use class information in the Interface Descriptors",
    0x01:	"Audio",
    0x02:	"Communications and Communications Device Class (CDC) Control",
    0x03:	"Human Interface Device (HID)",
    0x05:	"Physical",
    0x06:	"Image",
    0x07:	"Printer",
    0x08:	"Mass Storage (MSD)",
    0x09:	"8Hub",
    0x0A:	"CDC-Data",
    0x0B:	"Smart Card",
    0x0D:	"Content Security",
    0x0E:	"Video",
    0x0F:	"Personal Healthcare",
    0x10:	"Audio/Video Devices",
    0x11:	"Billboard Device Class",
    0xDC:	"Diagnostic Device",
    0x0E:	"Wireless Controller",
    0xEF:	"Miscellaneous",
    0xFE:	"Application Specific",
    0xFF:	"Vendor Specific"
  }
}
const interfaceClass = 0xFF; // Vendor


function App() {
  const [error, setError] = useState(null);
  const [device, setDevice] = useState(null);
  const isConnected = useMemo(() => device && device.open, [device, device?.open]);

  useEffect(() => {
    if (device) { return; }
    
    (async () => {
      let devices = await navigator.usb.getDevices();
      if (devices && devices.length) {
        setDevice(devices[0]);
        
        connectToDevice();
      }
    })();
  }, []);

  useEffect(() => {
    const onConnect = (e) => {
      if (device === null) {
        setDevice(e.device);
        connectToDevice();
      }
    };
    navigator.usb.addEventListener("connect", onConnect);
    return () => navigator.usb.removeEventListener("connect", onConnect);
  }, []);

  const onClick = async () => {
    if (!device) {
      setDevice(await navigator.usb.requestDevice({ filters: [{vendorId: 0xcafe}] }).catch(e => null));
      connectToDevice();
    } else {
      try {
        await device.close();
      } catch (e) {
        setError(e);
      } finally {
        setDevice(null);
      }
    }
  }

  const connectToDevice = async () => {
    if (device === null) {
      return;
    }
    
    try {
      setError(null);

      try {
        await device.open();
      } catch (e) {
        // await new Promise((res, rej) => setTimeout(res, 0));
        try { await device.close(); } catch (e) {}
        await device.open();
      }

      await device.selectConfiguration(1);

      const itf = device.configuration.interfaces.filter(itf => itf.alternate.interfaceClass === interfaceClass)[0];
      const endpointIn = itf.alternate.endpoints.filter(endpoint => endpoint.direction === "in")[0];
      const endpointOut = itf.alternate.endpoints.filter(endpoint => endpoint.direction === "out")[0];

      await device.claimInterface(itf.interfaceNumber);
      // await device.controlTransferOut({
      //   requestType: "vendor",
      //   recipient: "other",
      //   request: 0x22,
      //   value: 0x01,
      //   index: 3
      // });

      // await device.controlTransferOut({
      //   requestType: "class",
      //   recipient: "device",
      //   request: 0x22,
      //   value: 0x01,
      //   index: 0x00
      // });

      // device.controlTransferIn({
      //   requestType: "class",
      //   recipient: "device",
      //   request: 0x22,
      //   value: 0x01,
      //   index: 0x00
      // }, 0).then(result => {
      //   console.log("control data response", result);
      // });

      console.log(device);
      
      // await device.transferOut(endpointIn.endpointNumber, encoder.encode("Dies ist ein Teststring"));

      device.transferIn(endpointOut.endpointNumber, 10).then((result) => {
        const decoder = new TextDecoder();
        console.log("Received: " + decoder.decode(result.data));
      })
      .catch(e => {
        if (e.name === "AbortError") { /* Ignore */ }
        else { throw e; }
      })
      .catch(e => setError(e));



      // device.controlTransferIn(endpointIn.endpointNumber, 10).then(result => {
      //   const decoder = new TextDecoder();
      //   console.log("Received status: " + result.status + ", data: " + decoder.decode(result.data));
      // })
      // .catch(e => {
      //   if (e.name === "AbortError") { /* Ignore */ }
      //   else { throw e; }
      // })
      // .catch(e => setError(e));

    } catch (e) {
      setError(e);
    }
  };

  useEffect(() => { connectToDevice(); }, [device]);

  return (
    <div className="App">
      <header className="App-header">
        <div onClick={onClick} className={classNames("App-logo", isConnected && "connected")} alt="logo" />
        {error && <div className="error">{error.message}</div>}
        {device && <div style={{fontSize: '50%'}}>
          {device.configurations.map((config, i, configs) => <React.Fragment key={i}>
            {configs.length > 1 && <h1>{config.configurationName ?? "Configuration"} ({config.configurationValue})</h1>}
            {config.interfaces.map((itf,i) => <React.Fragment key={i}>
              <h2 className={classNames(itf.claimed && "claimed")}>Interface {itf.interfaceNumber}: {USB_CODES.classes[itf.alternate.interfaceClass] ?? ("Class " + itf.alternate.interfaceClass)}</h2>
              {itf.alternates.map((alt, i, alts) => <React.Fragment key={i}>
                {alts.length > 1 && <h3 className={classNames(alt.claimed && "claimed")}>Alternate {alt.alternateSetting} ({USB_CODES.classes[alt.interfaceClass] ?? ("Class " + alt.interfaceClass)}, Subclass {alt.interfaceSubclass}, Protocol {alt.interfaceProtocol})</h3>}
                {alt.endpoints.map((endpoint, i) => <React.Fragment key={i}>
                  <h4 className={classNames(endpoint.claimed && "claimed")}>Endpoint {endpoint.endpointNumber} ({endpoint.direction}, {endpoint.type}, packet size {endpoint.packetSize})</h4>
                  </React.Fragment>)}
                </React.Fragment>)}
            </React.Fragment>)}
          </React.Fragment>)}
        </div>}


        {device && <div>
        
          <input type="text" />
          {/* <button onClick={() => {device.controlOut}} /> */}
        
        </div>}
      </header>
    </div>
  );
}

export default App;
