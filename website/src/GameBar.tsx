import { Button, ButtonGroup, Divider, Dropdown, DropdownItem, DropdownMenu, DropdownTrigger, Popover, PopoverContent, PopoverTrigger, Slider, Switch, Table, TableBody, TableCell, TableColumn, TableHeader, TableRow } from "@nextui-org/react";
import { Buffer } from 'buffer';
import { crc16ccitt } from 'crc';
import { useCallback, useEffect, useState } from "react";
import { Power } from "react-bootstrap-icons";
import PingIntervalSlider from "./PingIntervalSlider";
import { DeviceInfo, game_config_t, led_effect_t } from "./util";


type GameBarProps = {
    deviceInfo: DeviceInfo,
    handleError: (e: any) => void;
};

const flashEffects = {
    [led_effect_t.EFFECT_NONE]: { label: "Keiner", description: "Buzzer Effekt deaktiviert" },
    [led_effect_t.EFFECT_FLASH_WHITE]: { label: "Weißer Blitz", description: "Buzzer blitzt weiß auf" },
    [led_effect_t.EFFECT_FLASH_BASE_COLOR]: { label: "Farbiger Blitz", description: "Buzzer blitzt in seiner eingestellten Farbe" }
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
        [deviceInfo, handleError]);


    return <div className="rounded-xl p-5 bg-slate-900 flex w-full flex-wrap md:flex-nowrap gap-4 items-center mb-10">
        <PingIntervalSlider deviceInfo={deviceInfo} handleError={handleError} />
        <Divider orientation="vertical" className="h-12" />
        {/* <span className="grow" /> */}
        <ButtonGroup>
            <Button color="default" variant="shadow" onPress={() => sendCommandToAll([0x31])}>Alle deaktivieren</Button>
            <Divider orientation="vertical" />
            <Button color="default" variant="shadow" onPress={() => sendCommandToAll([0x32])}>Alle aktivieren</Button>
        </ButtonGroup>
        <span className="grow" />
        <GameConfigButton deviceInfo={deviceInfo} handleError={handleError} />
        <Divider orientation="vertical" />
        <span className="grow" />
        <Button color="danger" variant="shadow" startContent={<Power />} onPress={() => sendCommandToAll([0x50])}>Alle ausschalten</Button>
    </div >;

}
export const ChevronDownIcon = () => (
    <svg fill="none" height="14" viewBox="0 0 24 24" width="14" xmlns="http://www.w3.org/2000/svg">
        <path d="M17.9188 8.17969H11.6888H6.07877C5.11877 8.17969 4.63877 9.33969 5.31877 10.0197L10.4988 15.1997C11.3288 16.0297 12.6788 16.0297 13.5088 15.1997L15.4788 13.2297L18.6888 10.0197C19.3588 9.33969 18.8788 8.17969 17.9188 8.17969Z" fill="currentColor" />
    </svg>
);
export default GameBar;


type GameConfigButtonProps = GameBarProps & {
};


function GameConfigButton(props: GameConfigButtonProps) {
    const { deviceInfo, handleError } = props;
    const { device } = deviceInfo;

    const [showPopover, setShowPopover] = useState(false);
    const [gameConfig, setGameConfig] = useState<game_config_t>({
        buzz_effect: led_effect_t.EFFECT_NONE,
        buzzer_active_time: 5000,
        deactivation_time_after_buzzing: 2000,
        can_buzz_while_other_is_active: false,
        must_release_before_pressing: true,
        crc: 0
    });

    const resetConfig = useCallback(async () => {
        await device.controlTransferIn({
            requestType: "vendor",
            recipient: "device",
            request: 0x10,
            value: 0x21,
            index: 0
        }, game_config_t.baseSize)
            .then(result => {
                if (result.data) {
                    const buf = Buffer.from(result.data.buffer);
                    const gameConfig = new game_config_t(buf, true);
                    setGameConfig(gameConfig);
                }
            })
            .catch(handleError);
    }, [device, handleError]);

    /* Reset config once at mount */
    useEffect(() => { resetConfig(); }, [resetConfig]);

    const onCancel = useCallback(async () => {
        resetConfig();
        setShowPopover(false);
    }, [resetConfig]);

    const onSave = useCallback(async () => {
        // console.log(game_config_t.raw(gameConfig).buffer.toString('hex'));
        await device.controlTransferOut({
            requestType: "vendor",
            recipient: "device",
            request: 0x10,
            value: 0x21,
            index: 0
        }, game_config_t.raw(gameConfig))
            // .then(result => {
            // console.log("sent game config: ", result);
            // })
            .catch(handleError);
    }, [device, gameConfig, handleError]);

    const updateGameConfig = useCallback((modGameConfig: (gc: game_config_t) => void) => {
        let ret = new game_config_t(game_config_t.raw(gameConfig));
        modGameConfig(ret);
        ret.crc = crc16ccitt(game_config_t.raw(ret).buffer.slice(0, game_config_t.baseSize - 2), 0xFFFF) ^ 0xFFFF;
        setGameConfig(ret);
    }, [gameConfig, setGameConfig]);

    return <Popover showArrow
        backdrop="blur"
        className="dark text-foreground"
        placement="bottom"
        // isDismissable={false}
        // isKeyboardDismissDisabled
        isOpen={showPopover}
        // shouldCloseOnInteractOutside={e => { console.log("xxx", e); return false; }}
        onOpenChange={s => setShowPopover(s)}
    >
        <PopoverTrigger>
            <Button variant="shadow">Spielkonfiguration</Button>
        </PopoverTrigger>
        <PopoverContent className="p-2 w-96">
            <Table removeWrapper hideHeader aria-label="Spielkonfiguration">
                <TableHeader>
                    <TableColumn>Konfiguration</TableColumn>
                </TableHeader>
                <TableBody>
                    <TableRow>
                        <TableCell>
                            <Slider
                                aria-label="Buzzer aktiv"
                                minValue={0}
                                maxValue={20000}
                                step={500}
                                value={gameConfig.buzzer_active_time}
                                onChange={v => updateGameConfig(gc => { gc.buzzer_active_time = v as number; })}
                                getValue={value => {
                                    value = Array.isArray(value) ? value[0] : value;
                                    if (value === 0) return "Sofort";
                                    else if (value === 65535) return "Nie";
                                    return `${value as number / 1000}s`;
                                }}
                                label="Buzzer aktiv"
                            />
                        </TableCell>
                    </TableRow>
                    <TableRow>
                        <TableCell>
                            <Slider
                                aria-label="Buzzer deaktiviert"
                                minValue={0}
                                maxValue={20000}
                                step={500}
                                value={gameConfig.deactivation_time_after_buzzing}
                                onChange={v => updateGameConfig(gc => { gc.deactivation_time_after_buzzing = v as number; })}
                                getValue={value => {
                                    value = Array.isArray(value) ? value[0] : value;
                                    if (value === 0) return "Nein";
                                    else if (value === 65535) return "Für immer";
                                    return `${value as number / 1000}s`;
                                }}
                                label="Buzzer deaktiviert"
                            />
                        </TableCell>
                    </TableRow>
                    <TableRow>
                        <TableCell>
                            <div className="mb-2">Buzzer-Effekt</div>
                            <Dropdown placement="bottom-end">
                                <DropdownTrigger>
                                    <Button className="w-full" variant="shadow">
                                        {flashEffects[gameConfig.buzz_effect]?.label ?? '?'}
                                        <ChevronDownIcon />
                                    </Button>
                                </DropdownTrigger>
                                <DropdownMenu
                                    disallowEmptySelection
                                    aria-label="Buzzer-Effekte"
                                    selectedKeys={[`${gameConfig.buzz_effect}`]}
                                    selectionMode="single"
                                    onSelectionChange={s => updateGameConfig(gc => { gc.buzz_effect = (s as Set<led_effect_t>).values().next().value; })}
                                    className="max-w-[300px]"
                                >
                                    {Object.entries(flashEffects).map(([k, v]) => <DropdownItem key={`${k}`} description={v.description}>
                                        {v.label}
                                    </DropdownItem>)}
                                </DropdownMenu>
                            </Dropdown>
                        </TableCell>
                    </TableRow>
                    <TableRow>
                        <TableCell>
                            <Switch
                                isSelected={gameConfig.can_buzz_while_other_is_active}
                                onValueChange={v => updateGameConfig(gc => { gc.can_buzz_while_other_is_active = v; })}
                            >Gleichzeitiges Drücken erlaubt</Switch>
                        </TableCell>
                    </TableRow>
                    <TableRow>
                        <TableCell>
                            <Switch
                                isSelected={gameConfig.must_release_before_pressing}
                                onValueChange={v => updateGameConfig(gc => { gc.must_release_before_pressing = v; })}
                            >Loslassen vor Drücken erforderlich</Switch>
                        </TableCell>
                    </TableRow>
                    <TableRow>
                        <TableCell className="pt-5 flex gap-4">
                            <pre className="text-xs self-end">0x{gameConfig.crc.toString(16).padStart(4, '0')}</pre>
                            <span className="grow" />
                            <Button variant="shadow" color="default" onPress={onCancel}>Abbrechen</Button>
                            <Button variant="shadow" color="success" onPress={onSave}>Speichern</Button>
                        </TableCell>
                    </TableRow>
                </TableBody>
            </Table>
        </PopoverContent>
    </Popover >;
}