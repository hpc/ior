#!/bin/bash
cd "${0%/*}"
if [[ ! -e run-all-tests.sh ]] ; then
	echo "Error, this script must run from the ./testing/docker directory"
	exit 1
fi

echo "Checking docker"
docker ps
if [ $? != 0 ] ; then
	echo "Error, cannot run docker commands"
	groups |grep docker || echo "You are not in the docker group !"
	exit 1
fi

echo "Building docker containers"

for IMAGE in $(find -type d | cut -b 3- |grep -v "^$") ; do
	docker build -t hpc/ior:$IMAGE $IMAGE
	if [ $? != 0 ] ; then
		echo "Error building image $IMAGE"
		exit 1
	fi
done
