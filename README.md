# HOW-TO: TUTORIAL ON PARA-SDK, BUILD AND RUN THE PROJECT


## 1. How to run the Docker file (the files will be in `1_PARA_SDK_utils`)

1. Run the `create_env.sh` (skip if you had the Docker Image installed):
```bash
# Add execute permission if needed
sudo chmod 777 create_env.sh 
# Run the script
./create_env.sh
``` 

2. Run the docker-compose file: 

```bash
# Install the docker-compose first 
sudo apt install docker-compose -y

# Run the docker-compose file 
cd /path-to-the-folder-have-docker-compose-yml/
docker-compose up -d
```
> Note: The `docker-compose.yml` is configured to the 
    `container_name: para-sdk-20251120`,
    `shm_size: "${SHM_SIZE:-2g}"`, 
    `ipv4_address`: `172.31.36.143`,
you can configure for your interest and your project. 

## How to build and run the project (the files will be in `2_Project_utils`)

This manual will help you to run the project easily 

1. Copy all the bash script is given by Dino (include `build`, `clean`, `kill-all` and `run`) into the project. 

```bash
cd /path-to-the-project/
sudo cp /path-to-bash-script/*.sh .
```

2. Build the project (MAKE SURE TO BUILD IT FIRST!!)

```bash
# Add this if you cannot run the scripts
sudo chmod 777 *.sh
# Build the project
./build.sh
```

3. Run it! 

```bash
./run.sh
```

4. If you want to clean the project: 

```bash
./clean.sh
```

> This script will clear all the build in `path-to-the-project/build`, so make sure dont leave anything behind!

5. Kill all the process related to the project (this script is experimental, you should use `htop` and disable manually, okie?): 

```bash
./kill-all.sh
```
