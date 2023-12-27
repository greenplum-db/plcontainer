## PL/Container

This is an implementation of trusted language execution engine capable of
bringing up Docker containers to isolate executors from the host OS, i.e.
implement sandboxing.

The architecture of PL/Container is described at [PL/Container-Architecture](https://github.com/greenplum-db/plcontainer/wiki/PLContainer-Architecture)

### Requirements

1. PL/Container runs on any linux distributions which support Greenplum Database.
1. PL/Container requires minimal Docker version 17.05.
1. GPDB version should be 5.2.0 or later. [For PostgreSQL](README_PG.md)

### Building PL/Container

Get the code repo
```shell
git clone https://github.com/greenplum-db/plcontainer.git
```

### Configue the build

Create the build directory:

```
cd plcontainer
mkdir build
```

To configure the build with the specific version of GPDB, either source the `greenplum_path.sh` first:

```
source /path/to/gpdb/greenplum_path.sh
cd build
cmake ..
```

Or pass the `pg_config` path through command line:

```
cd build
cmake .. -DPG_CONFIG=/path/to/gpdb/bin/pg_config
```

### Build & install

In the `build` directory, to build & install the plcontainer extension:

```
make
make install
```

To build the clients:

```
make clients
```

Use `make help` to see more build targets.


### Configuring PL/Container

To configure PL/Container environment, you need to enable PL/Container for specific databases by running:

```shell
psql -d your_database -c 'create extension plcontainer;'
```

### Running the regression tests

1. Prepare testing docker images for R & Python environment:

```
cd build
make images_artifact
```

1. Tests require some images and runtime configurations are installed.

Install the PL/Container R & Python docker images by running

```shell
plcontainer image-add -f plcontainer-python-image-<version>-gp<gpversion>.tar.gz
plcontainer image-add -f plcontainer-python2-image-<version>-gp<gpversion>.tar.gz
plcontainer image-add -f plcontainer-r-image-<version>-gp<gpversion>.tar.gz
```

Add runtime configurations as below

```shell
make prepare_runtime
```

1. Start tests:

```
make installcheck
```

### Unsupported feature
There some features PLContainer doesn't support. For unsupported feature list and their corresponding issue,
please refer to [Unsupported Feature](https://github.com/greenplum-db/plcontainer/wiki/PLContainer-Unsupported-Features)

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

## Debugging Locally

Sometimes, it is much eaiser to use a debugger like GDB to debug the clients. As they are typically run in containers, debugging info might not be loaded, such as the debug symbols of `libpython3.x.so`.

To make debugging easier, we can run the clients on the same host as the database backend process with the following steps:

1. Compile the client on host with bash command `cmake --build build/pyclient/ && make -C build/ install`.
1. Start a new database session and run SQL command `SET plcontainer.backend_type='process';`.
1. Start the client with `LOCAL_PROCESS_MODE=1 $GPHOME/bin/plcontainer_clients/py3client`.
1. Run the PL/Container UDF and the UDF will run in a process on host.

### Contributing
PL/Container is maintained by a core team of developers with commit rights to the [plcontainer repository](https://github.com/greenplum-db/plcontainer) on GitHub. At the same time, we are very eager to receive contributions and any discussions about it from anybody in the wider community.

Everyone interests PL/Container can [subscribe gpdb-dev](mailto:gpdb-dev+subscribe@greenplum.org) mailist list, send related topics to [gpdb-dev](mailto:gpdb-dev@greenplum.org), create issues or submit PR.

### License

The 'plcontainer' and 'pyclient' are distributed under the [BSD license](BSD LICENSE) and the [license described here](LICENSE).

---

With the exception of the 'rclient' source code is distributed under [GNU GPL v3](src/rclient/COPYING).
