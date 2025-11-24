export DOCKER_USER="edupopcornsar"
export DOCKER_PASSWORD="edupopcorn"
# [Host Shell] Set image repository and tag as environment variables
export IMAGE_REPO="edupopcornsar/pc-linux"
export IMAGE_TAG="R2011-v250829"
# e.g., salespopcornsar/pc-linux
# e.g., R2011-v250829
# [Host Shell] Docker Hub login using environment variables
echo $DOCKER_PASSWORD | docker login -u $DOCKER_USER --password-stdin
# [Host Shell] Download Docker image (using environment variables)
sudo docker pull ${IMAGE_REPO}:${IMAGE_TAG}