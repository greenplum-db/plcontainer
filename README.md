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

You can build PL/Container in a following way:

1. Go to the PL/Container directory: `cd /plcontainer`
1. plcontainer needs libcurl >=7.40. If the libcurl version on your system is low, you need to upgrade at first. For example, you could download source code and then compile and install, following this page: [Install libcurl from source](https://curl.haxx.se/docs/install.html). Note you should make sure the libcurl library path is in the list for library lookup. Typically you might want to add the path into LD_LIBRARY_PATH and export them in shell configuration or greenplum_path.sh on all nodes.
1. Make and install it: `make clean && make && make install`
1. Make with code coverae enabled: `make clean && make enable_coverage=true && make install`. After running test, generate code coverage report: `make coverage-report`

You need to restart database only for the first time you build&install plcontainer. Further rebuild plcontainer doesn't require database to be restarted to catch up new container execution library, you can simply connect to the database and all the calls you will make to plcontainer language would be held by new binaries you just built


### Configuring PL/Container

To configure PL/Container environment, you need to do the following steps (take python as an example):
1. Enable PL/Container for specific databases by running 
   `psql -d your_database -f $GPHOME/share/postgresql/plcontainer/plcontainer_install.sql`
1. Install the PL/Container image by running 
   `plcontainer image-add -i /home/gpadmin/plcontainer-python-images-1.0.0.tar.gz`
1. Configure the runtime by running
   `plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l python`

### Running the tests

1. Make sure your runtimes for Python and R are both added.
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

Everyone interests PL/Container can [subscribe gpdb-dev](mailto:gpdb-dev+subscribe@greenplum.org) mailist list and send related topics to [gpdb-dev](mailto:gpdb-dev@greenplum.org).
