## PL/Container

This is an implementation of trusted language execution engine capable of
bringing up Docker containers to isolate executors from the host OS, i.e.
implement sandboxing.

### Requirements

1. PL/Container runs on CentOS/RHEL 7.x or CentOS/RHEL 6.6+
1. PL/Container requires Docker version 17.05 for CentOS/RHEL 7.x and Docker version 1.7 CentOS/RHEL 6.6+
1. GPDB version should be 5.2.0 or later

### Building PL/Container Language

Get the code repo
```shell
git clone https://github.com/greenplum-db/plcontainer.git
```

You can build PL/Container in the following way:

1. Go to the PL/Container directory: `cd /plcontainer`
1. plcontainer needs libcurl >=7.40. If the libcurl version on your system is low, you need to upgrade at first. For example, you could download source code and then compile and install, following this page: [Install libcurl from source](https://curl.haxx.se/docs/install.html). Note you should make sure the libcurl library path is in the list for library lookup. Typically you might want to add the path into LD_LIBRARY_PATH and export them in shell configuration or greenplum_path.sh on all nodes.
1. Make and install it: `make clean && make && make install`
1. Make with code coverae enabled: `make clean && make enable_coverage=true && make install`. After running test, generate code coverage report: `make coverage-report`


### Configuring PL/Container

To configure PL/Container environment, you need to do the following steps (take python as an example):
1. Enable PL/Container for specific databases by running 
   ```shell
   psql -d your_database -f $GPHOME/share/postgresql/plcontainer/plcontainer_install.sql
   ```
1. Install the PL/Container image by running 
   ```shell
   plcontainer image-add -f /home/gpadmin/plcontainer-python-images.tar.gz
   ```
1. Configure the runtime by running
   ```shell
   plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes
   ```

### Running the tests

1. Tests require three runtimes(Python, R and Network) are added. Suppose you have installed the python runtime in the above section.

   Install the PL/Container R image by running
   ```shell
   plcontainer image-add -f /home/gpadmin/plcontainer-r-images.tar.gz;
   ```
   Add R runtime by running
   ```shell
   plcontainer runtime-add -r plc_r_shared -i pivotaldata/plcontainer_r_shared:devel -l r -s use_container_logging=yes;
   ```
   Add Network runtime by running
   ```shell
   plcontainer runtime-add -r plc_python_network -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s use_container_network=yes;
   ```
1. Go to the PL/Container test directory: `cd /plcontainer/tests`
1. Make it: `make tests`

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

1. The function definition starts with the line `# container: plc_python_share` which defines the name of runtime that will be used for running this function. To check the list of runtimes defined in the system you can run the command `plcontainer runtime-show`. Each runtime is mapped to a single docker image, you can list the ones available in your system with command `docker images`


For example, to define a function that uses a container that runs the `R`
interpreter, simply make the following definition:
```sql
CREATE FUNCTION dummyR() RETURNS text AS $$
# container: plc_r_shared
return (log10(100))
$$ LANGUAGE plcontainer;
```


### Contributing
PL/Container is maintained by a core team of developers with commit rights to the [plcontainer repository](https://github.com/greenplum-db/plcontainer) on GitHub. At the same time, we are very eager to receive contributions and any discussions about it from anybody in the wider community.

Everyone interests PL/Container can [subscribe gpdb-dev](mailto:gpdb-dev+subscribe@greenplum.org) mailist list, send related topics to [gpdb-dev](mailto:gpdb-dev@greenplum.org), create issues or submit PR.
