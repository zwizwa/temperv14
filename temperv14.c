/*
 * temperv14.c By Steffan Slot (dev-random.net) based on the version
 * below by Juan Carlos.
 *
 * pcsensor.c by Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Temper driver for linux. This program can be compiled either as a library
 * or as a standalone program (-DUNIT_TEST). The driver will work with some
 * TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY Juan Carlos Perez ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Robert kavaler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Modified from original version (0.0.1) by Patrick Skillen (pskillen@gmail.com)
 * - 2013-01-10 - If outputting only deg C or dec F, do not print the date (to make scripting easier)
 */
/*
 * 2013-08-19
 * Modified from abobe version for better name, and support to
 * subsctract degress in celsius for some TEMPer devices that displayed
 * too much.
 *
 */


/* 2018-02-01 simplified - Tom Schouten */
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(assertion) ({ \
            if(!(assertion)) { \
                LOG("%s: %d: ASSERT FAIL: " #assertion "\n", __FILE__, __LINE__); \
                assert_fail(); \
            } })



#include <usb.h>
#include <stdio.h>
#include <time.h>

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <setjmp.h>

#define VERSION "0.0.1"

#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401

#define INTERFACE1 0x00
#define INTERFACE2 0x01

const static int reqIntLen=8;
const static int endpoint_Int_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Int_out=0x00; /* endpoint 1 address for OUT */
const static int timeout=5000; /* timeout in ms */

const static char uTemperature[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni1[]        = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni2[]        = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

static int interrupt=0;
static int seconds=5;

jmp_buf error_jmp_buf;



void assert_fail() {
    //exit(1);
    longjmp(error_jmp_buf, 1);
}

usb_dev_handle *find_dev();

void usb_detach(usb_dev_handle *dev, int iInterface) {
    usb_detach_kernel_driver_np(dev, iInterface);
}

usb_dev_handle* setup_libusb_access() {
    usb_dev_handle *dev;
    usb_set_debug(0);
    usb_init();
    usb_find_busses();
    usb_find_devices();
    ASSERT(dev = find_dev());
    usb_detach(dev, INTERFACE1);
    usb_detach(dev, INTERFACE2);
    ASSERT(usb_set_configuration(dev, 0x01) >= 0);
    // Microdia tiene 2 interfaces
    ASSERT(usb_claim_interface(dev, INTERFACE1) >= 0);
    ASSERT(usb_claim_interface(dev, INTERFACE2) >= 0);
    return dev;
}

usb_dev_handle *find_dev() {
    struct usb_bus *bus;
    struct usb_device *dev;
    for (bus = usb_busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == VENDOR_ID &&
                dev->descriptor.idProduct == PRODUCT_ID ) {
                usb_dev_handle *handle;
                ASSERT(handle = usb_open(dev));
                return handle;
            }
        }
    }
    return NULL;
}

void ini_control_transfer(usb_dev_handle *dev) {
    unsigned char question[] = { 0x01,0x01 };
    ASSERT(usb_control_msg(dev, 0x21, 0x09, 0x0201, 0x00, (char*)question, 2, timeout) >= 0);
}

void control_transfer(usb_dev_handle *dev, const char *pquestion) {
    unsigned char question[reqIntLen];
    memcpy(question, pquestion, sizeof question);
    ASSERT(usb_control_msg(dev, 0x21, 0x09, 0x0200, 0x01, (char*)question, reqIntLen, timeout) >= 0);
}

void interrupt_transfer(usb_dev_handle *dev) {
    int i;
    char answer[reqIntLen];
    char question[reqIntLen];
    for (i=0;i<reqIntLen; i++) question[i]=i;
    ASSERT(usb_interrupt_write(dev, endpoint_Int_out, question, reqIntLen, timeout) >= 0);
    ASSERT(usb_interrupt_read(dev, endpoint_Int_in, answer, reqIntLen, timeout) == reqIntLen);
    usb_release_interface(dev, 0);
}

void interrupt_read(usb_dev_handle *dev) {
    char answer[reqIntLen];
    bzero(answer, reqIntLen);
    ASSERT(usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout) == reqIntLen);
}

int16_t interrupt_read_temperature(usb_dev_handle *dev) {
    unsigned char answer[reqIntLen];
    bzero(answer, reqIntLen);
    ASSERT(usb_interrupt_read(dev, 0x82, (char*)answer, reqIntLen, timeout) == reqIntLen);
    int16_t hi = (((char*)answer)[2]);
    int16_t lo = answer[3];
    return lo + hi*256;
}

void ex_program(int sig) {
    interrupt=1;
    (void) signal(SIGINT, SIG_DFL);
}

int open_socket(const char *host, int port) {
    struct hostent *hp = gethostbyname(host);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    memcpy((char *)&address.sin_addr,
           (char *)hp->h_addr, hp->h_length);
    address.sin_port = htons((u_short)port);
    address.sin_family = AF_INET;
    int sockfd;
    ASSERT((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0);
    socklen_t addrlen = sizeof(address);
    ASSERT(-1 != connect(sockfd, (struct sockaddr *) &address, addrlen));
    return sockfd;
}

void usb_cleanup(usb_dev_handle *dev) {
    usb_release_interface(dev, INTERFACE1);
    usb_release_interface(dev, INTERFACE2);
    usb_close(dev);
}


/* App state. */
int fd;
usb_dev_handle *dev;



int start(int argc, char **argv) {
    fd = (argc != 3) ? -1 : open_socket(argv[1], atoi(argv[2]));
    dev = NULL;
    ASSERT(dev = setup_libusb_access());
    (void) signal(SIGINT, ex_program);

    ini_control_transfer(dev);
    control_transfer(dev, uTemperature );
    interrupt_read(dev);
    control_transfer(dev, uIni1 );
    interrupt_read(dev);
    control_transfer(dev, uIni2 );
    interrupt_read(dev);
    interrupt_read(dev); // workaround

    do {
        // workaround: needs to be read twice, otherwise it hangs next time.
        control_transfer(dev, uTemperature );
        interrupt_read(dev);

        control_transfer(dev, uTemperature );
        int16_t dac = interrupt_read_temperature(dev);

        if (-1 == fd) {
            float c = ((float)dac) / 256.0;
            float f = 32 + c * 1.8;
            printf("%02x %.2f %.2f\n", dac, c, f);
        }
        else {
            ASSERT(send(fd, &dac, sizeof(dac), 0) == sizeof(dac));
        }

        if (!interrupt) sleep(seconds);

    } while (!interrupt);

    usb_cleanup(dev);
    close(fd);

    return 0;
}


int main(int argc, char **argv) {
    // Monitor loop
    do {
        if(!setjmp(error_jmp_buf)) {
            // TRY
            start(argc, argv);
        }
        else {
            // CATCH
            usb_cleanup(dev);
            close(fd);
            LOG("restarting in 3 seconds\n");
            sleep(3);
        }
    } while (!interrupt);
}
