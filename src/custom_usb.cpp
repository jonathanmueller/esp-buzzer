#include "custom_usb.h"

#ifdef CONFIG_TINYUSB_ENABLED

#include "Arduino.h"
#include "comm.h"
#include "tusb.h"
#include "esp32-hal-tinyusb.h"
#include <nvm.h>

/* The arduino macros are wrong and not compatible with the TinyUSB macros */
#undef REQUEST_STAGE_SETUP
#undef REQUEST_STAGE_DATA
#undef REQUEST_STAGE_ACK

// #define REQUEST_STAGE_SETUP CONTROL_STAGE_SETUP
// #define REQUEST_STAGE_DATA CONTROL_STAGE_DATA
// #define REQUEST_STAGE_ACK CONTROL_STAGE_ACK

USBCDC usb_cdc;
USBVendor Vendor;
USBHIDKeyboard Keyboard;

/* Hack to get to private member itf */
struct _USBVendor : public Stream {
    uint8_t itf;
};

// CDC Control Requests
#define REQUEST_SET_LINE_CODING        0x20
#define REQUEST_GET_LINE_CODING        0x21
#define REQUEST_SET_CONTROL_LINE_STATE 0x22

// CDC Line Coding Control Request Structure
typedef struct __attribute__((packed)) {
    uint32_t bit_rate;
    uint8_t stop_bits; // 0: 1 stop bit, 1: 1.5 stop bits, 2: 2 stop bits
    uint8_t parity;    // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
    uint8_t data_bits; // 5, 6, 7, 8 or 16
} request_line_coding_t;

static request_line_coding_t vendor_line_coding = { 9600, 0, 0, 8 };

// Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
static uint8_t vendor_line_state = 0;
#include "esp32-hal-uart.h"
#include "soc/uart_struct.h"
#include "hal/uart_ll.h"

static void ARDUINO_ISR_ATTR cdc_write(char c) {
    tud_cdc_n_write_char(0, c);
}

// USB and Vendor events
static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    switch (event_id) {
        case ARDUINO_USB_STARTED_EVENT:
            log_d("ARDUINO_USB_STARTED_EVENT");
            ets_install_putc2((void (*)(char)) & cdc_write);
            break;
        case ARDUINO_USB_STOPPED_EVENT:
            log_d("ARDUINO_USB_STOPPED_EVENT");
            ets_install_putc2(NULL);
            break;
        case ARDUINO_USB_SUSPEND_EVENT:
            log_d("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
            break;
        case ARDUINO_USB_RESUME_EVENT:
            log_d("USB RESUMED");
            break;

        default:
            break;
    }
}

enum { CDC_LINE_IDLE,
       CDC_LINE_1,
       CDC_LINE_2,
       CDC_LINE_3 };
static void usbCdcLineStateEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    arduino_usb_cdc_event_data_t *data = (arduino_usb_cdc_event_data_t *)event_data;

    static uint8_t lineState = CDC_LINE_IDLE;

    bool dtr = data->line_state.dtr;
    bool rts = data->line_state.rts;

    if (lineState == CDC_LINE_IDLE && dtr && rts) {
        lineState++;
    } else if (lineState == CDC_LINE_1 && dtr && !rts) {
        // log_i("Rebooting...");
        // esp_restart();
        // usb_cdc.enableReboot();
    } else {
        lineState = CDC_LINE_IDLE;
    }
    // log_d("line state event: dtr=%d, rts=%d, line state=%d", dtr, rts, lineState);
}

static void usbCdcLineCodingEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    arduino_usb_cdc_event_data_t *data = (arduino_usb_cdc_event_data_t *)event_data;
    if (data->line_coding.bit_rate == 1200) {
        log_d("1200bps touch. rebooting.");
    }
}

static void usbCdcRxData(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    arduino_usb_cdc_event_data_t *data = (arduino_usb_cdc_event_data_t *)event_data;
    log_d("USB CDC RX %d bytes:", data->rx.len);
    while (usb_cdc.available()) {
        usb_cdc.read();
    }
}

static void vendorDataCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    arduino_usb_vendor_event_data_t *data = (arduino_usb_vendor_event_data_t *)event_data;
    log_d("WebUSB RX %d bytes", data->data.len);

    while (Vendor.available()) {
        Vendor.write(Vendor.read());
    }

    // Workaround, because Vendor.flush() is not implemented
    tud_vendor_n_write_flush(((_USBVendor *)&Vendor)->itf);
}

enum USB_REQUEST_VENDOR_DEVICE : uint8_t {
    USB_REQUEST_VENDOR_DEVICE_VERSION      = 0x00,
    USB_REQUEST_VENDOR_DEVICE_CONFIG       = 0x10,
    USB_REQUEST_VENDOR_DEVICE_NETWORK_INFO = 0x20,
    USB_REQUEST_VENDOR_DEVICE_SEND_COMMAND = 0x30,
};

static const char *strRequestDirections[] = { "OUT", "IN" };
static const char *strRequestTypes[]      = { "STANDARD", "CLASS", "VENDOR", "INVALID" };
static const char *strRequestRecipients[] = { "DEVICE", "INTERFACE", "ENDPOINT", "OTHER" };
static const char *strRequestStages[]     = { "SETUP", "DATA", "ACK" };

// Handle USB requests to the vendor interface
bool vendorRequestCallback(uint8_t rhport, uint8_t requestStage, arduino_usb_control_request_t const *request) {
    // if (requestStage != CONTROL_STAGE_SETUP) {
    //     if (requestStage == REQUEST_STAGE_ACK) {
    //         log_d("ack %2x", request->bRequest);
    //     }
    //     return true;
    // }

    // if (requestStage == REQUEST_STAGE_DATA) {
    log_v("Vendor Request.\nStage: %5s, Direction: %3s, Type: %8s, Recipient: %9s, bRequest: 0x%02x, wValue: 0x%04x, wIndex: %u, wLength: %u",
          strRequestStages[requestStage],
          strRequestDirections[request->bmRequestDirection],
          strRequestTypes[request->bmRequestType],
          strRequestRecipients[request->bmRequestRecipient],
          request->bRequest, request->wValue, request->wIndex, request->wLength);
    // }

    bool result = false;

    if (request->bmRequestType == REQUEST_TYPE_VENDOR &&
        request->bmRequestRecipient == REQUEST_RECIPIENT_DEVICE) {
        switch ((USB_REQUEST_VENDOR_DEVICE)request->bRequest) {
            case USB_REQUEST_VENDOR_DEVICE_VERSION:
                static uint8_t versionCode = VERSION_CODE;

                if (request->bmRequestDirection == REQUEST_DIRECTION_OUT || request->wLength != sizeof(versionCode)) { return false; }
                if (requestStage != CONTROL_STAGE_SETUP) { return true; }

                result = Vendor.sendResponse(rhport, request, (void *)&versionCode, sizeof(versionCode));

                break;
            case USB_REQUEST_VENDOR_DEVICE_CONFIG:

                switch ((command_t)request->wValue) {
                    case COMMAND_SET_PING_INTERVAL:
                        if (requestStage != CONTROL_STAGE_SETUP) { return true; }

                        if (request->wLength != sizeof(pingInterval)) {
                            log_d("invalid length %d, expected %d", request->wLength, sizeof(pingInterval));
                            break;
                        }

                        result = Vendor.sendResponse(rhport, request, (void *)&pingInterval, sizeof(pingInterval));

                        if (request->bmRequestDirection == REQUEST_DIRECTION_OUT) {
                            log_v("received new pingInterval: %d", pingInterval);
                        } else {
                            log_v("sent pinginterval: %d", pingInterval);
                        }

                        break;

                    case COMMAND_SET_GAME_CONFIG:
                        if (request->wLength != sizeof(game_config_t)) {
                            log_d("invalid length %d, expected %d", request->wLength, sizeof(game_config_t));
                            break;
                        }

                        if (request->bmRequestDirection == REQUEST_DIRECTION_IN) {
                            if (requestStage != CONTROL_STAGE_SETUP) { return true; }
                            result = Vendor.sendResponse(rhport, request, (void *)&nvm_data.game_config, sizeof(game_config_t));
                        } else {
                            static payload_command_t game_config_command = {
                                .command = COMMAND_SET_GAME_CONFIG,
                            };

                            if (requestStage == CONTROL_STAGE_SETUP) {
                                result = Vendor.sendResponse(rhport, request, (void *)&game_config_command.args.game_config, sizeof(game_config_t));
                            } else if (requestStage == CONTROL_STAGE_ACK) {
                                result = true;

                                executeCommand(NULL, &game_config_command, sizeof(payload_command_t));
                                executeCommand(s_broadcast_mac, &game_config_command, sizeof(payload_command_t));

                            } else {
                                result = true;
                            }
                        }

                        break;
                    default:
                        result = false;
                        break;
                }
                break;
            case USB_REQUEST_VENDOR_DEVICE_NETWORK_INFO:
                if (requestStage != CONTROL_STAGE_SETUP) { return true; }

                if ((request->wLength != sizeof(peer_data_t) && request->wLength != sizeof(peer_data_table)) || request->wIndex >= PEER_DATA_TABLE_ENTRIES) {
                    log_v("invalid length %d, expected %d", request->wLength, sizeof(peer_data_t));
                    break;
                }

                log_v("sending %d byte response", sizeof(peer_data_t));
                cleanup_peer_list();
                if (request->wLength == sizeof(peer_data_t)) {
                    result = Vendor.sendResponse(rhport, request, &peer_data_table[request->wIndex], sizeof(peer_data_t));
                } else if (request->wLength == sizeof(peer_data_table)) {
                    result = Vendor.sendResponse(rhport, request, &peer_data_table, sizeof(peer_data_table));
                }
                break;
            case USB_REQUEST_VENDOR_DEVICE_SEND_COMMAND:
                if (request->wLength < 7 || request->bmRequestDirection != REQUEST_DIRECTION_OUT) {
                    break;
                }

                static struct {
                    uint8_t dst_mac_addr[6];
                    payload_command_t command;
                } __attribute__((packed)) command_to_send;
                result = true;

                if (requestStage == CONTROL_STAGE_SETUP) {
                    result = Vendor.sendResponse(rhport, request, &command_to_send, request->wLength);
                } else if (requestStage == CONTROL_STAGE_ACK) {
                    executeCommand(command_to_send.dst_mac_addr, &command_to_send.command, request->wLength - sizeof(command_to_send.dst_mac_addr));
                }

                break;
            default:
                result = false;
                break;
        }
    } else if (request->bmRequestDirection == REQUEST_DIRECTION_OUT &&
               request->bmRequestType == REQUEST_TYPE_STANDARD &&
               request->bmRequestRecipient == REQUEST_RECIPIENT_INTERFACE &&
               request->bRequest == 0x0b) {
        if (requestStage == CONTROL_STAGE_SETUP) {
            // response with status OK
            result = Vendor.sendResponse(rhport, request);
        } else {
            result = true;
        }
    } else
        // Implement CDC Control Requests
        if (request->bmRequestType == REQUEST_TYPE_CLASS && request->bmRequestRecipient == REQUEST_RECIPIENT_DEVICE) {
            switch (request->bRequest) {
                case REQUEST_SET_LINE_CODING: // 0x20
                    // Accept only direction OUT with data size 7
                    if (request->wLength != sizeof(request_line_coding_t) || request->bmRequestDirection != REQUEST_DIRECTION_OUT) {
                        break;
                    }
                    if (requestStage == CONTROL_STAGE_SETUP) {
                        // Send the response in setup stage (it will write the data to vendor_line_coding in the DATA stage)
                        result = Vendor.sendResponse(rhport, request, (void *)&vendor_line_coding, sizeof(request_line_coding_t));
                    } else if (requestStage == CONTROL_STAGE_ACK) {
                        // In the ACK stage the response is complete
                        log_i("Vendor Line Coding: bit_rate: %lu, data_bits: %u, stop_bits: %u, parity: %u\n", vendor_line_coding.bit_rate, vendor_line_coding.data_bits, vendor_line_coding.stop_bits, vendor_line_coding.parity);
                    }
                    result = true;
                    break;

                case REQUEST_GET_LINE_CODING: // 0x21
                    // Accept only direction IN with data size 7
                    if (request->wLength != sizeof(request_line_coding_t) || request->bmRequestDirection != REQUEST_DIRECTION_IN) {
                        break;
                    }
                    if (requestStage == CONTROL_STAGE_SETUP) {
                        // Send the response in setup stage (it will write the data to vendor_line_coding in the DATA stage)
                        result = Vendor.sendResponse(rhport, request, (void *)&vendor_line_coding, sizeof(request_line_coding_t));
                    }
                    result = true;
                    break;

                case REQUEST_SET_CONTROL_LINE_STATE: // 0x22
                    // Accept only direction OUT with data size 0
                    if (request->wLength != 0 || request->bmRequestDirection != REQUEST_DIRECTION_OUT) {
                        break;
                    }
                    if (requestStage == CONTROL_STAGE_SETUP) {
                        // Send the response in setup stage
                        vendor_line_state = request->wValue;
                        result            = Vendor.sendResponse(rhport, request);
                        Vendor.flush();
                    } else if (requestStage == CONTROL_STAGE_ACK) {
                        // In the ACK stage the response is complete
                        bool dtr = (vendor_line_state & 1) != 0;
                        bool rts = (vendor_line_state & 2) != 0;
                        log_i("Vendor Line State: dtr: %u, rts: %u\n", dtr, rts);
                    }
                    result = true;
                    break;

                default:
                    // stall unknown request
                    break;
            }
        }

    return result;
}

void usb_setup_task(void *arg) {
    // delay(4000);

    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_PANIC) {
        // log_e("Delaying USB initialization...");
        // delay(5000);
        // vTaskDelete(NULL);
        // return;
    }

    log_i("Setting up USB. Switching log output...");

    usb_cdc.enableReboot(true);
    usb_cdc.onEvent(ARDUINO_USB_CDC_LINE_STATE_EVENT, usbCdcLineStateEvent);
    // usb_cdc.onEvent(ARDUINO_USB_CDC_LINE_CODING_EVENT, usbCdcLineCodingEvent);
    usb_cdc.onEvent(ARDUINO_USB_CDC_RX_EVENT, usbCdcRxData);

    Vendor.onEvent(ARDUINO_USB_VENDOR_DATA_EVENT, vendorDataCallback);
    Vendor.onRequest(vendorRequestCallback);

    USB.onEvent(usbEventCallback);

    usb_cdc.begin();
    Vendor.begin();
    USB.begin();
    Keyboard.begin();

    // usb_cdc.setDebugOutput(true);

    // while (true) {
    //     usb_cdc.print(".");
    //     Vendor.print(",");
    //     delay(1000);
    // }

    // vTaskDelete(NULL);
}

void usb_setup() {
    usb_setup_task(NULL);
    // xTaskCreate(usb_setup_task, "usb_setup", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
}

#endif