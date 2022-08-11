FROM centos:8

WORKDIR /data
RUN sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
RUN sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
RUN yum update -y
RUN yum install -y mpich openmpi git pkg-config nano gcc bzip2 patch gcc-c++ make mpich-devel openmpi-devel
RUN yum install -y sudo
RUN yum install -y autoconf automake
