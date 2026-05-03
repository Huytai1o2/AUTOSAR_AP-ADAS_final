#include <linux/kernel.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>

#define BAUD_RATE 115200

/*
Product: GP02 GPS
Baudrate: 115200 (modified)
Peripheral: UART0
Tx: GPIO14 (Pin 8)
Rx: GPIO15 (Pin 10)
VCC: 3.3V
Author: l1ttled1no
*/


// This function triggers whenever the GP-02 sends NMEA data over UART
static size_t gp02_receive_buf(struct serdev_device *serdev, const unsigned char *buf, size_t count)
{
    // GPS modules send NMEA sentences (ASCII). 
    // We print it as text. Using %.*s limits the string to 'count' characters.
    pr_info("GP-02 GPS Data: %.*s\n", (int)count, buf);
    
    return count;
}

static const struct serdev_device_ops gp02_ops = {
    .receive_buf = gp02_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

static int gp02_probe(struct serdev_device *serdev)
{
    int status;

    dev_info(&serdev->dev, "Probing GP-02 GPS driver...\n");

    serdev_device_set_client_ops(serdev, &gp02_ops);

    status = serdev_device_open(serdev);
    if (status) {
        dev_err(&serdev->dev, "Error opening serial port for GP-02!\n");
        return status;
    }

    // Updated to match your configured GP-02 baud rate
    serdev_device_set_baudrate(serdev, BAUD_RATE);
    serdev_device_set_flow_control(serdev, false);

    dev_info(&serdev->dev, "GP-02 GPS UART initialized successfully at %d baud.\n", BAUD_RATE);
    return 0;
}

static void gp02_remove(struct serdev_device *serdev)
{
    serdev_device_close(serdev);
    dev_info(&serdev->dev, "GP-02 GPS driver removed.\n");
}

// The "compatible" string binds this driver to the Device Tree Overlay
static const struct of_device_id gp02_of_match[] = {
    { .compatible = "makerlab,gp02-gps", },
    { }
};
MODULE_DEVICE_TABLE(of, gp02_of_match);

static struct serdev_device_driver gp02_driver = {
    .probe = gp02_probe,
    .remove = gp02_remove,
    .driver = {
        .name = "gp02_gps_uart",
        .of_match_table = gp02_of_match,
    },
};

module_serdev_device_driver(gp02_driver);

MODULE_AUTHOR("l1ttled1no");
MODULE_DESCRIPTION("GP-02 GPS UART Serdev Driver");
MODULE_LICENSE("GPL");


