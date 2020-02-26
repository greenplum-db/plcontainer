## PL/Container - Greenplum PL Analysis Suit Backend (Beta version)

This is an implementation of trusted language execution engine capable of
bringing up Docker containers to isolate executors from the host OS. PL/Container provides
a safe, controllable and manageable execution environment procedure language for Greenplum Database (GPDB). 

Note: This is a new version of PL/Conatienr and still under development, only R language is supported. For old version of PL/Container, please refer the following link

PL/Container for Python/Python3/R [link](https://github.com/greenplum-db/plcontainer/tree/6X_STABLE)
PL/Container for Greenplum Database 5 [link](https://github.com/greenplum-db/plcontainer/tree/5X_STABLE)

For R frontend, please refer the [Greenplum R](https://github.com/greenplum-db/GreenplumR). 


### Requirements

1. PL/Container runs on CentOS/RHEL 7.x or Ubuntu 18.04
1. PL/Container requires newest Docker version with API 1.27 support.
1. Greenplum Database version should be 6.1.0 or later.
1. PL/Container requires system libcurl with Unix Domain Socket support.
1. PL/Container requires R version 3.3.3+ and R_HOME is set.

### Build and install PL/Container GPDB backend client

#### Get the code repo

```shell
git clone --recursive https://github.com/greenplum-db/plcontainer.git
```

#### Install the third-party libraries

`Protobuf and gRpc`

```shell
git clone --recursive --branch v1.24.3 --depth 1 https://github.com/grpc/grpc.git
cd grpc/third_party/protobuf
./autogen.sh
./configure
make -j
sudo make install
export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH
cd ../..
make -j
sudo make install
```

#### Build and Install

```shell
cd plcontainer
make docker-dep
make proto
make
make install
```

#### Build and install PL/Container GPDB backend sever

```
cd plcontainer/src/rclient
make copy-proto
make
make clone-gtest
make test
cd ../..
make install-clients
```

### Configuring PL/Container

#### Start coordinator to manage containers

```
gpconfig -c shared_preload_libraries -v 'plc_coordinator'
gpstop -arf
```

#### Enable PL/Container extension in GPDB

To configure PL/Container environment, you need to enable PL/Container for specific databases by running 
   ```shell
   psql -d your_database -c 'create extension plcontainer;'
   ```

### Running the regression tests

1. Prepare docker images for R & Python environment.
   Refer [How to build docker image](https://github.com/greenplum-db/plcontainer/wiki/How-to-build-docker-image) for docker file examples.
 You can also download PLContainer images from [pivotal networks](https://network.pivotal.io) 

1. Tests require some images and runtime configurations are installed.

   Install the PL/Container R & Python docker images by running
   ```shell
   plcontainer image-add -f /home/gpadmin/plcontainer-r-images.tar.gz;
   ```

   Add runtime configurations as below
   ```shell
   plcontainer runtime-add -r plc_r_shared -i pivotaldata/plcontainer_r_shared:devel -l r
   ```

1. Go to the PL/Container test directory: `cd plcontainer/tests`
1. Make it: `make tests`

Note that if you just want to test or run your own R or Python code, you do just need to install the image and runtime for that language.

### Unsupported feature
Some feaures and new language support is still under developing.

### Why new PL/Container

1. New PL/Container uses less system resource
1. New PL/Container provides more flexibility to manage container
1. New PL/Container provides more features/functions to debug user code
1. New PL/Container has better performance
1. New PL/Container is more robust

### Example

The idea of PL/Container is to use containers to run user defined functions. The current implementation assume the PL function definition to have the following structure:

```sql
CREATE FUNCTION dummyR() RETURNS text AS $$
# container: plc_r_shared
return ('hello from R')
$$ LANGUAGE plcontainer;
```

There are a couple of things you need to pay attention to:

1. The `LANGUAGE` argument to Greenplum is `plcontainer`

1. The function definition starts with the line `# container: plc_r_shared` which defines the name of runtime that will be used for running this function. To check the list of runtimes defined in the system you can run the command `plcontainer runtime-show`. Each runtime is mapped to a single docker image, you can list the ones available in your system with command `docker images`

PL/Container supports various parameters for docker run, and also it supports some useful UDFs for monitoring or debugging. Please read the official document for details. 

### Contributing
PL/Container is maintained by a core team of developers with commit rights to the [plcontainer repository](https://github.com/greenplum-db/plcontainer) on GitHub. At the same time, we are very eager to receive contributions and any discussions about it from anybody in the wider community.

Everyone interests PL/Container can [subscribe gpdb-dev](mailto:gpdb-dev+subscribe@greenplum.org) mailist list, send related topics to [gpdb-dev](mailto:gpdb-dev@greenplum.org), create issues or submit PR.
