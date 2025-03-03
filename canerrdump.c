////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//  canerrdump - utility to display SocketCAN error messages, by Zeljko Avramovic (c) 2024        //
//                                                                                                //
//  SPDX-License-Identifier: LGPL-2.1-or-later OR BSD-3-Clause                                    //
//                                                                                                //
//  Virtual CAN adapter vcan0 can be set up like this:                                            //
//  sudo modprobe vcan                                                                            //
//  sudo ip link add dev vcan0 type vcan                                                          //
//  sudo ip link set vcan0 mtu 72              # needed for CAN FD                                //
//  sudo ip link set vcan0 up                                                                     //
//                                                                                                //
//  To simulate error messages use canerrsim utility like this:                                   //
//  ./canerrsim vcan0 LostArBit=09 Data4=AA TX BusOff NoAck ShowBits                              //
//                                                                                                //
//  That should show in canerrdump utility as:                                                    //
//  0x06A [8] 09 00 80 00 AA 00 00 00  ERR=LostArBit09,NoAck,BusOff,Prot(Type(TX),Loc(Unspec))    //
//                                                                                                //
//  Alternatively, you could use candump from can-utils to check only error messages like this:   //
//  candump -tA -e -c -a any,0~0,#FFFFFFFF                                                        //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdint.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>

#define can_interface_name argv[1]
#define STR_EQUAL 0

void show_help_and_exit() {
    printf("\n");
    printf("Usage: canerrdump <CAN interface> [Options]\n");
    printf("\n");
    printf("CAN interface:           ( CAN interface is case sensitive )\n");
    printf("    can0                 ( or can1, can2 or virtual ones like vcan0, vcan1...\n");
    printf("\n");
    printf("Options:                 ( options are not case sensitive )\n");
    printf("                         ( ERROR CLASS (MASK) IN CAN ID: )\n");
    printf("    IgnoreTxTimeout      ( filter TX timeout by netdevice driver error messages )\n");
    printf("    IgnoreLostArbit      ( filter lost arbitration error messages )\n");
    printf("    IgnoreController     ( filter controller problem error messages )\n");
    printf("    IgnoreProtocol       ( filter protocol error messages )\n");
    printf("    IgnoreTransceiver    ( filter transceiver status error messages )\n");
    printf("    IgnoreNoAck          ( filter no ACK on transmission error messages )\n");
    printf("    IgnoreBusOff         ( filter bus off error messages )\n");
    printf("    IgnoreBusError       ( filter bus error messages )\n");
    printf("    IgnoreRestarted      ( filter controller restarted messages )\n");
    printf("    IgnoreCounters       ( filter TX and RX error counter messages )\n");
    printf("                         ( DEBUG HELPERS: )\n");
    printf("    ShowBits             ( display all error filtering bits )\n");
    printf("\n");
    printf("Examples:\n");
    printf("\n");
    printf("    ./canerrdump can1 ShowBits\n");
    printf("    ( dump all CAN error messages from CAN interface can1 and show error filtering bit mask )\n");
    printf("\n");
    printf("    ./canerrdump vcan0 IgnoreNoAck IgnoreBusOff\n");
    printf("    ( dump all CAN error messages from virtual CAN interface vcan0 except NoACk and BusOff)\n");
    printf("\n");
    exit(EXIT_SUCCESS);
}

void err_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_binary(uint32_t number) {
    uint32_t mask = 0x80000000; // start with the most significant bit
    for (int i = 0; i < 32; i++) {
        putchar((number & mask) ? '1' : '0');
        mask >>= 1; // shift the mask to the right
    }
}

void remove_trailing_comma(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == ',')
        str[len - 1] = '\0';   // replace the last character with the null terminator to shorten the string
}



int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    // struct can_filter filter;
    can_err_mask_t errmask;
    bool show_bits = false;
    char err_str[1024];
    char buf[256];

    printf("CAN Sockets Error Messages Dumper\n");
    if (argc < 2)
        show_help_and_exit();

    //filter.can_id = CAN_INV_FILTER;

    errmask       = CAN_ERR_FLAG   // include only error frames
                  | CAN_ERR_MASK;  // show all possible error frames

    if (argc >= 3) {               // Parse command line parameters
        for (size_t i = 2; i < argc; i++) {
            // str_to_upper(argv[i]);
            if (strcasecmp(argv[i], "IgnoreTxTimeout")        == STR_EQUAL)
                errmask &= ~CAN_ERR_TX_TIMEOUT; // Exclude TxTimeout errors
            else if (strcasecmp(argv[i], "IgnoreLostArbit")   == STR_EQUAL)
                errmask &= ~CAN_ERR_LOSTARB;   // Exclude LostArbitration errors
            else if (strcasecmp(argv[i], "IgnoreController")  == STR_EQUAL)
                errmask &= ~CAN_ERR_CRTL;      // Exclude Controller errors
            else if (strcasecmp(argv[i], "IgnoreProtocol")    == STR_EQUAL)
                errmask &= ~CAN_ERR_PROT;      // Exclude Protocol errors
            else if (strcasecmp(argv[i], "IgnoreTransveiver") == STR_EQUAL)
                errmask &= ~CAN_ERR_TRX;       // Exclude Transceiver errors
            else if (strcasecmp(argv[i], "IgnoreNoAck")       == STR_EQUAL)
                errmask &= ~CAN_ERR_ACK;       // Exclude NoAck errors
            else if (strcasecmp(argv[i], "IgnoreBusOff")      == STR_EQUAL)
                errmask &= ~CAN_ERR_BUSOFF;    // Exclude BusOff errors
            else if (strcasecmp(argv[i], "IgnoreBusError")    == STR_EQUAL)
                errmask &= ~CAN_ERR_BUSERROR;  // Exclude BusError errors
            else if (strcasecmp(argv[i], "IgnoreRestarted")   == STR_EQUAL)
                errmask &= ~CAN_ERR_RESTARTED; // Exclude Restarted errors
            else if (strcasecmp(argv[i], "IgnoreCounters")    == STR_EQUAL)
                errmask &= ~CAN_ERR_CNT;       // Exclude TX and RX counter errors
            else if (strcasecmp(argv[i], "ShowBits")          == STR_EQUAL)
                show_bits = true;              // Display all error mask filtering bits
            else {
                printf("Error: Invalid option: %s\n", argv[i]);
                //show_help_and_exit();
                exit(EXIT_SUCCESS);
            }
        }
    }

    if (show_bits == true) {
        //printf("filter.can_id = ");
        //print_binary(filter.can_id);
        //printf("\n");
        printf("Error Mask = ");
        print_binary(errmask);        
        printf("\n");
    }
    
    // create socket
    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
        err_exit("Error while opening socket");

    // set interface name
    strcpy(ifr.ifr_name, can_interface_name); // can0, vcan0...
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        sprintf(buf, "Error setting CAN interface name %s", can_interface_name);        
        err_exit(buf);
    }

    // bind socket to the CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        err_exit("Error in socket bind");
    
    //setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER,     &filter,  sizeof(filter));
    setsockopt(sock, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &errmask, sizeof(errmask));

    printf("Listening CAN bus %s for errors...\n", can_interface_name);

    while (1) {
        ssize_t nbytes = read(sock, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("Error reading CAN frame");
            return 1;
        } else if (nbytes < sizeof(struct can_frame)) {
            fprintf(stderr, "Incomplete CAN frame\n");
            continue;
        }

        if (frame.can_id & CAN_ERR_FLAG) {   // check if it's an error frame
            // 0x040 [8] 00 00 00 00 00 00 00 00
            if (frame.can_id & CAN_EFF_FLAG) // extended or standard frame
                printf("0x%08X [%d] ", frame.can_id & CAN_ERR_MASK, frame.can_dlc);
            else
                printf("0x%03X [%d] ", frame.can_id & CAN_ERR_MASK, frame.can_dlc);
            for (size_t i = 0; i < frame.can_dlc; i++)
                 printf("%02X ", frame.data[i]);
            printf(" ERR=");
            sprintf(err_str, "");

            if (frame.can_id & CAN_ERR_TX_TIMEOUT)
                strcat(err_str, "TxTimeout,");

            if (frame.can_id & CAN_ERR_LOSTARB) {
                sprintf(buf, "LostArBit%02d,", frame.data[0]);
                strcat(err_str, buf);
            }   

            if (frame.can_id & CAN_ERR_ACK)
                strcat(err_str, "NoAck,");

            if (frame.can_id & CAN_ERR_BUSOFF)
                strcat(err_str, "BusOff,");

            if (frame.can_id & CAN_ERR_BUSERROR)
                strcat(err_str, "BusError,");

            if (frame.can_id & CAN_ERR_RESTARTED)
                strcat(err_str, "Restarted,");

            if (frame.can_id & CAN_ERR_CNT) {
                sprintf(buf, "Count(TX=%d,RX=%d),", frame.data[6], frame.data[7]);
                strcat(err_str, buf);
            }

            if (frame.can_id & CAN_ERR_CRTL) {                 // error status of CAN-controller               / data[1]
                sprintf(buf, "Ctrl(%s%s%s%s%s%s%s%s",
                        frame.data[1] & CAN_ERR_CRTL_RX_OVERFLOW ? "OverflowRX," : "",
                        frame.data[1] & CAN_ERR_CRTL_TX_OVERFLOW ? "OverflowTX," : "",
                        frame.data[1] & CAN_ERR_CRTL_RX_WARNING  ? "WarningRX,"  : "",
                        frame.data[1] & CAN_ERR_CRTL_TX_WARNING  ? "WarningTX,"  : "",
                        frame.data[1] & CAN_ERR_CRTL_RX_PASSIVE  ? "PassiveRX,"  : "",
                        frame.data[1] & CAN_ERR_CRTL_TX_PASSIVE  ? "PassiveTX,"  : "",
                        frame.data[1] & CAN_ERR_CRTL_ACTIVE      ? "Active"      : "",
                        frame.data[1] == CAN_ERR_CRTL_UNSPEC     ? "Unspec"      : "");
                remove_trailing_comma(buf);
                strcat(buf, "),");     
                strcat(err_str, buf);
            }

            if (frame.can_id & CAN_ERR_PROT) {                 // error in CAN protocol
                sprintf(buf, "Prot(Type(%s%s%s%s%s%s%s%s%s",   // error in CAN protocol (type)                 / data[2]
                        frame.data[2] & CAN_ERR_PROT_BIT      ? "SingleBit,"          : "",
                        frame.data[2] & CAN_ERR_PROT_FORM     ? "FrameFormat,"        : "",
                        frame.data[2] & CAN_ERR_PROT_STUFF    ? "BitStuffing,"        : "",
                        frame.data[2] & CAN_ERR_PROT_BIT0     ? "Bit0,"               : "",
                        frame.data[2] & CAN_ERR_PROT_BIT1     ? "Bit1,"               : "",
                        frame.data[2] & CAN_ERR_PROT_OVERLOAD ? "BusOverload,"        : "",
                        frame.data[2] & CAN_ERR_PROT_ACTIVE   ? "ActiveAnnouncement," : "",
                        frame.data[2] & CAN_ERR_PROT_TX       ? "TX"                  : "",
                        frame.data[2] == CAN_ERR_PROT_UNSPEC  ? "Unspec"              : "");
                remove_trailing_comma(buf);
                strcat(err_str, buf);
                sprintf(buf, "),Loc(%s)),",                    // error in CAN protocol (location)             / data[3]
                        frame.data[3] == CAN_ERR_PROT_LOC_UNSPEC  ? "Unspec" :
                        frame.data[3] == CAN_ERR_PROT_LOC_SOF     ? "SOF" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ID28_21 ? "ID28_21" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ID20_18 ? "ID20_18" :
                        frame.data[3] == CAN_ERR_PROT_LOC_SRTR    ? "SRTR" :
                        frame.data[3] == CAN_ERR_PROT_LOC_IDE     ? "IDE" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ID17_13 ? "ID17_13" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ID12_05 ? "ID12_05" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ID04_00 ? "ID04_00" :
                        frame.data[3] == CAN_ERR_PROT_LOC_RTR     ? "RTR" :
                        frame.data[3] == CAN_ERR_PROT_LOC_RES1    ? "RES1" :
                        frame.data[3] == CAN_ERR_PROT_LOC_RES0    ? "RES0" :
                        frame.data[3] == CAN_ERR_PROT_LOC_DLC     ? "DLC" :
                        frame.data[3] == CAN_ERR_PROT_LOC_DATA    ? "DATA" :
                        frame.data[3] == CAN_ERR_PROT_LOC_CRC_SEQ ? "CRC_SEQ" :
                        frame.data[3] == CAN_ERR_PROT_LOC_CRC_DEL ? "CRC_DEL" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ACK     ? "ACK" :
                        frame.data[3] == CAN_ERR_PROT_LOC_ACK_DEL ? "ACK_DEL" :
                        frame.data[3] == CAN_ERR_PROT_LOC_EOF     ? "EOF" :
                        frame.data[3] == CAN_ERR_PROT_LOC_INTERM  ? "INTERM" : "Unknown");
                strcat(err_str, buf);
            }            

            if (frame.can_id & CAN_ERR_TRX) {                  // error status of CAN-transceiver              / data[4]
                sprintf(buf, "Trans(%s),",                    
                        frame.data[4] == CAN_ERR_TRX_UNSPEC             ? "Unspec" :
                        frame.data[4] == CAN_ERR_TRX_CANH_NO_WIRE       ? "CanHiNoWire" :
                        frame.data[4] == CAN_ERR_TRX_CANH_SHORT_TO_BAT  ? "CanHiShortToBAT" :
                        frame.data[4] == CAN_ERR_TRX_CANH_SHORT_TO_VCC  ? "CanHiShortToVCC" :
                        frame.data[4] == CAN_ERR_TRX_CANH_SHORT_TO_GND  ? "CanHiShortToGND" :
                        frame.data[4] == CAN_ERR_TRX_CANL_NO_WIRE       ? "CanLoNoWire" :
                        frame.data[4] == CAN_ERR_TRX_CANL_SHORT_TO_BAT  ? "CanLoShortToBAT" :
                        frame.data[4] == CAN_ERR_TRX_CANL_SHORT_TO_VCC  ? "CanLoShortToVCC" :
                        frame.data[4] == CAN_ERR_TRX_CANL_SHORT_TO_GND  ? "CanLoShortToGND" :
                        frame.data[4] == CAN_ERR_TRX_CANL_SHORT_TO_CANH ? "CanLoShortToCanHi" : "Unknown");
                strcat(err_str, buf);
            }

            remove_trailing_comma(err_str);     
            printf("%s\n", err_str);
        }
    }

    close(sock);
    return 0;
}
