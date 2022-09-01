FROM ubuntu:22.04

WORKDIR /data
RUN apt-get update
RUN apt-get install -y libopenmpi-dev openmpi-bin libhdf5-openmpi-dev git pkg-config gcc libaio-dev libpnetcdf-dev 
RUN apt-get install -y sudo make
RUN git clone https://github.com/hpc/ior.git
RUN cd ior ; ./bootstrap
RUN cd ior ; ./configure  --with-aio --with-hdf5 --with-ncmpi LDFLAGS=-L/usr/lib/x86_64-linux-gnu/hdf5/openmpi CFLAGS=-I/usr/include/hdf5/openmpi && make -j

# librados-dev 
