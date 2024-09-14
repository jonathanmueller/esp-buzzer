import classNames from 'classnames';
import { useCallback, useState } from 'react';
import BuzzerDevice from './adapters/BuzzerDevice';
import './App.css';
import { DevicePeerList } from './DevicePeerList';
import DeviceSelector from './DeviceSelector';
import GameBar from './GameBar';


function App() {
  const [error, setError] = useState<Error>();
  const [device, setDevice] = useState<BuzzerDevice>();

  // const deviceRef = useRef(device);
  // const [deviceVersion, setDeviceVersion] = useState<number>();
  const [refreshDevice, setRefreshDevice] = useState({});

  // const [btDevice, setBTDevice] = useState<BluetoothDevice>();
  // const btDeviceRef = useRef(btDevice);


  const handleError = useCallback((e: Error) => {
    if (e.name === "AbortError") { /* Ignore */ return; }

    // if (e.name === "NetworkError" && e.message.indexOf("Unable to claim interface") !== -1) {
    //   e = new Error("Device is already connected in another process");
    // }
    console.error(e);

    setError(e);
  }, [setError]);

  const onConnectionStateChanged = useCallback(() => {
    setRefreshDevice({});
  }, [setRefreshDevice]);



  if (!navigator.usb && !navigator.bluetooth) {
    return <div className='dark:bg-gray-800 px-5'>
      <div className="mx-auto pt-10 container flex flex-col min-h-screen">
        <div className="rounded-xl p-5 bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">
          <div className={classNames("connect-icon h-[5rem] my-[-0.5rem] self-center", "bg-red-900")} />
          <p className='mx-auto text-red-300 text-2xl font-extralight'>Dieser Browser wird leider nicht unterstützt.</p>
        </div>
      </div>
    </div>;
  }


  return (
    <div className='dark:bg-gray-800 px-5'>
      <div className="mx-auto pt-10 container flex flex-col min-h-screen">

        <div className="rounded-xl p-5 bg-slate-500 dark:bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">
          {/* <div onClick={onClick} className={classNames("connect-icon h-[5rem] my-[-0.5rem] self-center transition hover:scale-110", deviceError && "bg-red-600", !deviceError && deviceInfo.isConnected && "bg-green-600")} /> */}

          <DeviceSelector onSelect={setDevice} onConnectionStateChanged={onConnectionStateChanged} />
          {device && <>
            <GameBar device={device} handleError={handleError} />
          </>}
        </div>

        <div className="text-red-600 font-bold text-center my-5">{error?.message ?? '\u00a0'}</div>


        {device && device.isConnected() && <div className="grid grid-cols-12 gap-3 flex-1">
          <div className="flex-1 col-span-12">
            <DevicePeerList device={device} handleError={handleError} />
          </div>

          {/* <div className="col-span-4 overflow-x-auto h-full">
            <Button onPress={() => device.transferOut(deviceInfo.endpointOut.endpointNumber, new Uint8Array([0xCA, 0xFE])).then(console.log)}>Send Data</Button>
            <DeviceLogViewer deviceInfo={deviceInfo} handleError={handleError} />
          </div> */}
        </div>}


      </div>

    </div>
  );
}

export default App;
