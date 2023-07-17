/*************************************************************************
 * Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *********************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PRINT_ENABLED 1

#include "bacnet/apdu.h"
#include "bacnet/arf.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacerror.h"
#include "bacnet/bactext.h"
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/config.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/iam.h"
#include "bacnet/npdu.h"
#include "bacnet/version.h"
#include "bacnet/whois.h"
#include "bacport.h"

// Some demo stuff needed
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/filename.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/datalink/dlenv.h"

// Custom Func Prototypes
void custom_rp_ack_print(BACNET_READ_PROPERTY_DATA *data);
int read_prop(uint32_t dev_inst, uint32_t obj_type, BACNET_OBJECT_TYPE obj_inst, BACNET_PROPERTY_ID obj_prop, int32_t prop_ind);

// String to hold last chunk of data read; used to capture list length for iteration
static char data_str[256];

// Buffer used for receive
static uint8_t Rx_Buf[MAX_MPDU] = {0};

// Converted command line arguments
static uint32_t Device_Instance = BACNET_MAX_INSTANCE;
static uint32_t Object_Instance = BACNET_MAX_INSTANCE;
static BACNET_OBJECT_TYPE Object_Type = OBJECT_ANALOG_INPUT;
static BACNET_PROPERTY_ID Object_Property = PROP_ACKED_TRANSITIONS;
static int32_t Property_Index = BACNET_ARRAY_ALL;

// The invoke id is needed to filter incoming messages
static uint8_t Request_Invoke_ID = 0;
static BACNET_ADDRESS Target_Address;
static bool Error_Detected = false;
static bool found;
static BACNET_ADDRESS src = {0}; // Address where message came from
static time_t timeout_seconds;
static unsigned max_apdu = 0;


static void MyErrorHandler(BACNET_ADDRESS *src, uint8_t invoke_id, BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code) {
    if (address_match(&Target_Address, src) && (invoke_id == Request_Invoke_ID)) {
        printf("BACnet Error: %s: %s\n", bactext_error_class_name((int)error_class), bactext_error_code_name((int)error_code));
        Error_Detected = true;
    }
}


static void MyAbortHandler(BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t abort_reason, bool server) {
    (void)server;
    if (address_match(&Target_Address, src) && (invoke_id == Request_Invoke_ID)) {
        printf("BACnet Abort: %s\n", bactext_abort_reason_name((int)abort_reason));
        Error_Detected = true;
    }
}


static void MyRejectHandler(BACNET_ADDRESS *src, uint8_t invoke_id, uint8_t reject_reason) {
    if (address_match(&Target_Address, src) && (invoke_id == Request_Invoke_ID)) {
        printf("BACnet Reject: %s\n", bactext_reject_reason_name((int)reject_reason));
        Error_Detected = true;
    }
}


//Original is rp_ack_print_data() from line 47 of h_rp_a.c
void custom_rp_ack_print(BACNET_READ_PROPERTY_DATA *data) {
    BACNET_OBJECT_PROPERTY_VALUE object_value; /* for bacapp printing */
    BACNET_APPLICATION_DATA_VALUE value; /* for decode value data */
    int len = 0;
    uint8_t *application_data;
    int application_data_len;
    bool first_value = true;
    bool print_brace = false;

    if(data) {
        application_data = data->application_data;
        application_data_len = data->application_data_len;
        /* FIXME: what if application_data_len is bigger than 255? */
        /* value? need to loop until all of the len is gone... */
        for(;;) {
            len = bacapp_decode_application_data(application_data, (uint8_t)application_data_len, &value);
            if(first_value && (len < application_data_len)) {
                first_value = false;
                fprintf(stdout, "{");
                print_brace = true;
            }
            object_value.object_type = data->object_type;
            object_value.object_instance = data->object_instance;
            object_value.object_property = data->object_property;
            object_value.array_index = data->array_index;
            object_value.value = &value;
            bacapp_print_value(stdout, &object_value);

            /* int status = */bacapp_snprintf_value(data_str, 256, &object_value);

            if(len > 0) {
                if(len < application_data_len) {
                    application_data += len;
                    application_data_len -= len;
                    /* there's more! */
                    fprintf(stdout, ",");
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
        if(print_brace)
            fprintf(stdout, "}");
        fprintf(stdout, "\r\n");
    }
}


/** Handler for a ReadProperty ACK.
 * @ingroup DSRP
 * Doesn't actually do anything, except, for debugging, to
 * print out the ACK data of a matching request.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
 */
static void My_Read_Property_Ack_Handler(uint8_t *service_request, uint16_t service_len, BACNET_ADDRESS *src, BACNET_CONFIRMED_SERVICE_ACK_DATA *service_data) {
    int len = 0;
    BACNET_READ_PROPERTY_DATA data;

    if (address_match(&Target_Address, src) && (service_data->invoke_id == Request_Invoke_ID)) {
        len = rp_ack_decode_service_request(service_request, service_len, &data);
        if (len < 0) {
            printf("<decode failed!>\n");
        }
        else {
            custom_rp_ack_print(&data);
        }
    }
}


static void Init_Service_Handlers(void)
{
    Device_Init(NULL);

    // We need to handle who-is to support dynamic device binding to us
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);

    // Handle i-am to support binding to other devices
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);

    // Set the handler for all the services we don't implement. It is required to send the proper reject message...
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);

    // We must implement read property - it's required!
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);

    // Handle the data coming back from confirmed requests
    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROPERTY, My_Read_Property_Ack_Handler);

    // Handle any errors coming back
    apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROPERTY, MyErrorHandler);
    apdu_set_abort_handler(MyAbortHandler);
    apdu_set_reject_handler(MyRejectHandler);
}


static void print_usage(char *filename) {
    printf("Usage: %s device-instance object-type object-instance "
           "property [index]\n",
           filename);
    printf("       [--dnet][--dadr][--mac]\n");
    printf("       [--version][--help]\n");
}


static void print_help(char *filename) {
    printf("Read a property from an object in a BACnet device\n"
           "and print the value.\n");
    printf("--mac A\n"
           "Optional BACnet mac address."
           "Valid ranges are from 00 to FF (hex) for MS/TP or ARCNET,\n"
           "or an IP string with optional port number like 10.1.2.3:47808\n"
           "or an Ethernet MAC in hex like 00:21:70:7e:32:bb\n"
           "\n"
           "--dnet N\n"
           "Optional BACnet network number N for directed requests.\n"
           "Valid range is from 0 to 65535 where 0 is the local connection\n"
           "and 65535 is network broadcast.\n"
           "\n"
           "--dadr A\n"
           "Optional BACnet mac address on the destination BACnet network "
           "number.\n"
           "Valid ranges are from 00 to FF (hex) for MS/TP or ARCNET,\n"
           "or an IP string with optional port number like 10.1.2.3:47808\n"
           "or an Ethernet MAC in hex like 00:21:70:7e:32:bb\n"
           "\n");
    printf("device-instance:\n"
           "BACnet Device Object Instance number that you are\n"
           "trying to communicate to.  This number will be used\n"
           "to try and bind with the device using Who-Is and\n"
           "I-Am services.  For example, if you were reading\n"
           "Device Object 123, the device-instance would be 123.\n"
           "\nobject-type:\n"
           "The object type is object that you are reading. It\n"
           "can be defined either as the object-type name string\n"
           "as defined in the BACnet specification, or as the\n"
           "integer value of the enumeration BACNET_OBJECT_TYPE\n"
           "in bacenum.h. For example if you were reading Analog\n"
           "Output 2, the object-type would be analog-output or 1.\n"
           "\nobject-instance:\n"
           "This is the object instance number of the object that\n"
           "you are reading.  For example, if you were reading\n"
           "Analog Output 2, the object-instance would be 2.\n"
           "\nproperty:\n"
           "The property of the object that you are reading. It\n"
           "can be defined either as the property name string as\n"
           "defined in the BACnet specification, or as an integer\n"
           "value of the enumeration BACNET_PROPERTY_ID in\n"
           "bacenum.h. For example, if you were reading the Present\n"
           "Value property, use present-value or 85 as the property.\n"
           "\nindex:\n"
           "This integer parameter is the index number of an array.\n"
           "If the property is an array, individual elements can\n"
           "be read.  If this parameter is missing and the property\n"
           "is an array, the entire array will be read.\n"
           "\nExample:\n"
           "If you want read the Present-Value of Analog Output 101\n"
           "in Device 123, you could send either of the following\n"
           "commands:\n"
           "%s 123 analog-output 101 present-value\n"
           "%s 123 1 101 85\n"
           "If you want read the Priority-Array of Analog Output 101\n"
           "in Device 123, you could send either of the following\n"
           "commands:\n"
           "%s 123 analog-output 101 priority-array\n"
           "%s 123 1 101 87\n",
           filename, filename, filename, filename);
}


int read_prop(uint32_t dev_inst, uint32_t obj_type, BACNET_OBJECT_TYPE obj_inst, BACNET_PROPERTY_ID obj_prop, int32_t prop_ind) {

    unsigned timeout = 100; // Milliseconds
    uint16_t pdu_len = 0;
    Request_Invoke_ID = 0;
    Error_Detected = false;
    time_t last_seconds = time(NULL);
    time_t current_seconds = 0;
    time_t elapsed_seconds = 0;

    for (;;) {

        // Increment timer - exit if timed out
        current_seconds = time(NULL);

        // At least one second has passed
        if (current_seconds != last_seconds) {
            tsm_timer_milliseconds((uint16_t)((current_seconds - last_seconds) * 1000));
        }

        if (Error_Detected) { break; }

        // Wait until the device is bound, or timeout and quit
        if (!found) {
            found = address_bind_request(dev_inst, &max_apdu, &Target_Address);
        }

        if (found) {
            if (Request_Invoke_ID == 0) {
                Request_Invoke_ID = Send_Read_Property_Request(dev_inst, obj_type, obj_inst, obj_prop, prop_ind);
            }
            else if (tsm_invoke_id_free(Request_Invoke_ID)) {
                break;
            }
            else if (tsm_invoke_id_failed(Request_Invoke_ID)) {
                fprintf(stderr, "\rError: TSM Timeout!\n");
                tsm_free_invoke_id(Request_Invoke_ID);
                Error_Detected = true;
                // Try again or abort?
                break;
            }
        }
        else {
            // Increment timer - exit if timed out
            elapsed_seconds += (current_seconds - last_seconds);
            if (elapsed_seconds > timeout_seconds) {
                printf("\rError: APDU Timeout!\n");
                Error_Detected = true;
                break;
            }
        }

        // Returns 0 bytes on timeout
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);

        // Process
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }

        // Keep track of time for next check
        last_seconds = current_seconds;
    }

    return Error_Detected;
}


int main(int argc, char *argv[]) {

    BACNET_MAC_ADDRESS mac = { 0 };
    BACNET_ADDRESS dest = { 0 };
    bool using_mac = false;

    // bool specific_address = false;

    long dnet = -1;
    BACNET_MAC_ADDRESS adr = {0};
    int argi = 0;
    unsigned int target_args = 0;
    char *filename = NULL;

    filename = filename_remove_path(argv[0]);
    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--help") == 0) {
            print_usage(filename);
            print_help(filename);
            return 0;
        }
        if (strcmp(argv[argi], "--version") == 0) {
            printf("%s %s\n", filename, BACNET_VERSION_TEXT);
            printf("Copyright (C) 2015 by Steve Karg and others.\n"
                   "This is free software; see the source for copying "
                   "conditions.\n"
                   "There is NO warranty; not even for MERCHANTABILITY or\n"
                   "FITNESS FOR A PARTICULAR PURPOSE.\n");
            return 0;
        }
        // if (strcmp(argv[argi], "--mac") == 0) {
        //     if (++argi < argc) {
        //         if (address_mac_from_ascii(&mac, argv[argi])) {
        //             specific_address = true;
        //         }
        //     }
        // }
        // else if (strcmp(argv[argi], "--dnet") == 0) {
        //     if (++argi < argc) {
        //         dnet = strtol(argv[argi], NULL, 0);
        //         if ((dnet >= 0) && (dnet <= BACNET_BROADCAST_NETWORK)) {
        //             specific_address = true;
        //         }
        //     }
        // }
        // else if (strcmp(argv[argi], "--dadr") == 0) {
        //     if (++argi < argc) {
        //         if (address_mac_from_ascii(&adr, argv[argi])) {
        //             specific_address = true;
        //         }
        //     }
        // }
        else {
            if (target_args == 0) {

                // device object instance parameter can be of multiple types
                //
                // These disable WhoIs/IAm from occurring (IP[:Port])
                //  - n.n.n.n:p
                //  - n.n.n.n
                //  - n:n:n:n:n:n (hex-style encoding of IP:Port)
                //
                // This requires WhoIs/IAm for resolving DeviceID to IP address (DeviceID)
                //  - n

                // At least one ./: needed for IP/MAC formats
                if ((strchr(argv[argi], '.') != NULL) || (strchr(argv[argi], ':') != NULL)) {
                    using_mac = true;
                    Device_Instance = 1337;
                }
                // Otherwise DeviceID
                else {
                    Device_Instance = strtol(argv[argi], NULL, 0);
                }
                target_args++;
            }
            else if (target_args == 1) {
                if (bactext_object_type_strtol(argv[argi], &Object_Type) == false) {
                    fprintf(stderr, "object-type=%s invalid\n", argv[argi]);
                    return 1;
                }
                target_args++;
            }
            else if (target_args == 2) {
                Object_Instance = strtol(argv[argi], NULL, 0);
                target_args++;
            }
            else if (target_args == 3) {
                if (bactext_property_strtol(argv[argi], &Object_Property) == false) {
                    fprintf(stderr, "property=%s invalid\n", argv[argi]);
                    return 1;
                }
                target_args++;
            }
            else if (target_args == 4) {
                Property_Index = strtol(argv[argi], NULL, 0);
                target_args++;
            }
            else {
                print_usage(filename);
                return 1;
            }
        }
    }
    if (target_args < 4) {
        print_usage(filename);
        return 0;
    }
    if (Device_Instance > BACNET_MAX_INSTANCE) {
        fprintf(stderr, "device-instance=%u - it must be less than %u\n", Device_Instance, BACNET_MAX_INSTANCE);
        return 1;
    }
    address_init();
    // if (specific_address) {
    //     if (adr.len && mac.len) {
    //         memcpy(&dest.mac[0], &mac.adr[0], mac.len);
    //         dest.mac_len = mac.len;
    //         memcpy(&dest.adr[0], &adr.adr[0], adr.len);
    //         dest.len = adr.len;
    //         if ((dnet >= 0) && (dnet <= BACNET_BROADCAST_NETWORK)) {
    //             dest.net = dnet;
    //         }
    //         else {
    //             dest.net = BACNET_BROADCAST_NETWORK;
    //         }
    //     }
    //     else if (mac.len) {
    //         memcpy(&dest.mac[0], &mac.adr[0], mac.len);
    //         dest.mac_len = mac.len;
    //         dest.len = 0;
    //         if ((dnet >= 0) && (dnet <= BACNET_BROADCAST_NETWORK)) {
    //             dest.net = dnet;
    //         }
    //         else {
    //             dest.net = 0;
    //         }
    //     }
    //     else {
    //         if ((dnet >= 0) && (dnet <= BACNET_BROADCAST_NETWORK)) {
    //             dest.net = dnet;
    //         }
    //         else {
    //             dest.net = BACNET_BROADCAST_NETWORK;
    //         }
    //         dest.mac_len = 0;
    //         dest.len = 0;
    //     }
    //     address_add(Device_Instance, MAX_APDU, &dest);
    // }

    // If an IP/MAC format was specified instead of DeviceID - populate cache with that target (& fake ID 1337)
    if (using_mac) {
        address_mac_from_ascii(&mac, argv[1]);
        memcpy(&dest.mac[0], &mac.adr[0], mac.len);
        dest.mac_len = mac.len;
        dest.len = 0;
        dest.net = 0;
        address_add(Device_Instance, MAX_APDU, &dest);
    }

    // Setup my info
    Device_Set_Object_Instance_Number(BACNET_MAX_INSTANCE);
    Init_Service_Handlers();
    dlenv_init();
    atexit(datalink_cleanup);

    // Configure the timeout values
    timeout_seconds = (apdu_timeout() / 1000) * apdu_retries();

    // -----------------------------------------------------

    found = address_bind_request(Device_Instance, &max_apdu, &Target_Address);
    if (!found) {
        Send_WhoIs(Device_Instance, Device_Instance);
    }

    int ret;

    // index = -1: All indices
    //          0: Get length
    //       1..N: Specific index
    if (Property_Index != -2) {
        ret = read_prop(Device_Instance, Object_Type, Object_Instance, Object_Property, Property_Index);
    }

    // index = -2: Read length & Then read each index
    else {
        read_prop(Device_Instance, Object_Type, Object_Instance, Object_Property, 0);
        int len = atoi(data_str);
        int i;
        for(i = 1; i <= len; i++) {
            printf("%d: ", i);
            read_prop(Device_Instance, Object_Type, Object_Instance, Object_Property, i);
        }
        ret = 0;
    }

    return ret;
}
