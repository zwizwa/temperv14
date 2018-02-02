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
                exit(1); \
            } })



#include <usb.h>
#include <stdio.h>
#include <time.h>

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#define VERSION "0.0.1"

#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401

#define INTERFACE1 0x00
#define INTERFACE2 0x01

const static int reqIntLen=8;
const static int reqBulkLen=8;
const static int endpoint_Int_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Int_out=0x00; /* endpoint 1 address for OUT */
const static int endpoint_Bulk_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Bulk_out=0x00; /* endpoint 1 address for OUT */
const static int timeout=5000; /* timeout in ms */

const static char uTemperatura[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni1[] = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni2[] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

static int bsalir=1;
static int seconds=5;



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
    char question[] = { 0x01,0x01 };
    ASSERT(usb_control_msg(dev, 0x21, 0x09, 0x0201, 0x00, (char *) question, 2, timeout) >= 0);
}

void control_transfer(usb_dev_handle *dev, const char *pquestion) {
    char question[reqIntLen];
    memcpy(question, pquestion, sizeof question);
    ASSERT(usb_control_msg(dev, 0x21, 0x09, 0x0200, 0x01, (char *) question, reqIntLen, timeout) >= 0);
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

void interrupt_read_temperatura(usb_dev_handle *dev, float *tempC) {
    int temperature;
    char answer[reqIntLen];
    bzero(answer, reqIntLen);
    ASSERT(usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout) == reqIntLen);
    temperature = (answer[3] & 0xFF) + (answer[2] << 8);
    *tempC = temperature * (125.0 / 32000.0);
}

void bulk_transfer(usb_dev_handle *dev) {
    char answer[reqBulkLen];
    ASSERT(usb_bulk_write(dev, endpoint_Bulk_out, NULL, 0, timeout) >= 0);
    ASSERT(usb_bulk_read(dev, endpoint_Bulk_in, answer, reqBulkLen, timeout) == reqBulkLen);
    usb_release_interface(dev, 0);
}

void ex_program(int sig) {
    bsalir=1;
    (void) signal(SIGINT, SIG_DFL);
}

int main( int argc, char **argv) {

    usb_dev_handle *dev = NULL;
    float tempc;

    ASSERT(dev = setup_libusb_access());

    (void) signal(SIGINT, ex_program);

    ini_control_transfer(dev);

    control_transfer(dev, uTemperatura );
    interrupt_read(dev);

    control_transfer(dev, uIni1 );
    interrupt_read(dev);

    control_transfer(dev, uIni2 );
    interrupt_read(dev);
    interrupt_read(dev);

    do {
        // Workaround: needs to be read twice, otherwise it hangs next time.
        control_transfer(dev, uTemperatura );
        interrupt_read_temperatura(dev, &tempc);

        control_transfer(dev, uTemperatura );
        interrupt_read_temperatura(dev, &tempc);

        printf("%.2f\n", tempc);
        printf("%.2f\n", tempc);

        if (!bsalir)
            sleep(seconds);
    } while (!bsalir);

    usb_release_interface(dev, INTERFACE1);
    usb_release_interface(dev, INTERFACE2);

    usb_close(dev);

    return 0;
}
