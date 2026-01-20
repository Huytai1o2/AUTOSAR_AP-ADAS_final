# Running AUTOSAR on Raspberry Pi

This guide explains how to deploy and run the `AUTOSAR` application on a Raspberry Pi target.

## Prerequisites

1.  **Build openCV**: You should install openCV on github (you should follow [this script](scripts/build_opencv.sh))
2.  **Import LiteRT native arm sdk**: You should import LiteRT native arm sdk (on github project or build LiteRT on PI) in:
    ```bash
    root@6cb062f1ee34:~/workspace/HPC_Machine_v5/build# ls -la /opt 
    total 24
    drwxr-xr-x 1 root root 4096 Jan 19 16:14 .
    drwxr-xr-x 1 root root 4096 Jan 20 06:37 ..
    drwxr-xr-x 9 root root 4096 Aug 31  2024 cross-pi-gcc-12.2.0-64
    drwxrwxr-x 6 1000 1000 4096 Jan 16 09:08 litert_arm_sdk_native_pi
    drwxr-xr-x 9 root root 4096 Dec  1 06:19 para-sdk
    drwxr-xr-x 9 root root 4096 Dec  1 06:19 para-sdk_raspi-raspios64_bookworm
    ```
3.  **Build the Project**: First, you should include all the libs of para-sdk and liteRT sdk via cmake (for example, i install lib for cmake for whole project: [wholeProject cmake](CMakeLists.txt) and cmake of providerSensor like [providerSensor cmake](ProviderSensor/src/CMakeLists.txt))After that, ensure you have cross-compiled the project using `build_raspi.sh`.
4.  **Transfer Files**: Copy the `install` directory from your build host to the Raspberry Pi.
    *   **Example**: Compress the `install` directory and transfer it to the Raspberry Pi:
        ```bash
        tar -czvf install_package.tar.gz install
        scp install_package.tar.gz <user>@<pi_ip_address>:/home/huytai102/
        ```

## Execution Steps

1.  **Navigate to the install directory** on the Raspberry Pi:
    ```bash
    cd /home/huytai102/install
    ```

2.  **Set the Library Path**:
    You must add the local `lib` directory to `LD_LIBRARY_PATH` so the application can find `libpara_core` and other dependencies.
    ```bash
    export LD_LIBRARY_PATH=/home/huytai102/install/lib:$LD_LIBRARY_PATH
    ```

3. **Run the Application**:
    Before run, you should change the ip of example project into actual edge device ip like that:
    ```bash
    grep "unicast-address" ./etc/com/someip/*.json
    sed -i "s/<old_ip>/<actual_ip>/g" ./etc/com/someip/*.json
    ```

    Then, you must run:
    ```bash
    ./para-exec.sh
    ```
