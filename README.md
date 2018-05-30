## PL/Container

This is an implementation of trusted language execution engine capable of
bringing up Docker containers to isolate executors from the host OS, i.e.
implement sandboxing.

The architecture of PL/Container is described at [PL/Container-Architecture](https://github.com/greenplum-db/plcontainer/wiki/PLContainer-Architecture)

### Requirements

1. PL/Container runs on CentOS/RHEL 7.x or CentOS/RHEL 6.6+
1. PL/Container requires Docker version 17.05 for CentOS/RHEL 7.x and Docker version 1.7 CentOS/RHEL 6.6+
1. GPDB version should be 5.2.0 or later.    [ For PostgreSQL ](README_PG.md)

### Building PL/Container

Get the code repo
```shell
git clone https://github.com/greenplum-db/plcontainer.git
```

You can build PL/Container in the following way:

1. Go to the PL/Container directory: `cd plcontainer`
1. PL/Container needs libcurl >=7.40. If the libcurl version on your system is low, you need to upgrade at first. For example, you could download source code and then compile and install, following this page: [Install libcurl from source](https://curl.haxx.se/docs/install.html). Note you should make sure the libcurl library path is in the list for library lookup. Typically you might want to add the path into LD_LIBRARY_PATH and export them in shell configuration or greenplum_path.sh on all nodes (Note you need to restart the Greenplum cluster).
1. Make and install it: `make clean && make && make install`
1. Make with code coverage enabled (For dev and test only): `make clean && make ENABLE_COVERAGE=yes && make install`. After running test, generate code coverage report: `make coverage-report`


### Configuring PL/Container

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
   plcontainer image-add -f /home/gpadmin/plcontainer-python-images.tar.gz
   ```

   Add runtime configurations as below
   ```shell
   plcontainer runtime-add -r plc_r_shared -i pivotaldata/plcontainer_r_shared:devel -l r
   plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l python
   ```

1. Go to the PL/Container test directory: `cd plcontainer/tests`
1. Make it: `make tests`

Note that if you just want to test or run your own R or Python code, you do just need to install the image and runtime for that language.

### Design

The idea of PL/Container is to use containers to run user defined functions. The current implementation assume the PL function definition to have the following structure:

```sql
CREATE FUNCTION dummyPython() RETURNS text AS $$
# container: plc_python_shared
return 'hello from Python'
$$ LANGUAGE plcontainer;
```

There are a couple of things you need to pay attention to:

1. The `LANGUAGE` argument to Greenplum is `plcontainer`

1. The function definition starts with the line `# container: plc_python_shared` which defines the name of runtime that will be used for running this function. To check the list of runtimes defined in the system you can run the command `plcontainer runtime-show`. Each runtime is mapped to a single docker image, you can list the ones available in your system with command `docker images`

PL/Container supports various parameters for docker run, and also it supports some useful UDFs for monitoring or debugging. Please read the official document for details. 

### Contributing
PL/Container is maintained by a core team of developers with commit rights to the [plcontainer repository](https://github.com/greenplum-db/plcontainer) on GitHub. At the same time, we are very eager to receive contributions and any discussions about it from anybody in the wider community.

Everyone interests PL/Container can [subscribe gpdb-dev](mailto:gpdb-dev+subscribe@greenplum.org) mailist list, send related topics to [gpdb-dev](mailto:gpdb-dev@greenplum.org), create issues or submit PR.
