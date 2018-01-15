## PL/Container

This is an implementation of trusted language execution engine capable of
bringing up Docker containers to isolate executors from the host OS, i.e.
implement sandboxing.

### Requirements

1. PL/Container runs on CentOS/RHEL 7.x as this is a prerequisite for Docker
1. PL/Container requires Docker v1.10+ as it is built with Docker API v1.22
You can find more information in the user guide

### Building Environment

```shell
git clone https://github.com/greenplum-db/plcontainer.git
cd plcontainer/vagrant/centos
vagrant up
```

That's it, Greenplum is built and installed, database is up and running,
PL/Container code is available in `/plcontainer` directory inside of VM

### Building PL/Container Language

You can build PL/Container in a following way:

1. Login to Vagrant: `vagrant ssh`
1. Go to the PL/Container directory: `cd /plcontainer`
1. plcontainer needs libcurl >=7.40. If the libcurl version on your system is low, you need to upgrade at first. For example, you could download source code and then compile and install, following this page: [Install libcurl from source](https://curl.haxx.se/docs/install.html). Note you should make sure the libcurl library path is in the list for library lookup. Typically you might want to add the path into LD_LIBRARY_PATH and export them in shell configuration or greenplum_path.sh on all nodes.
1. Make and install it: `make clean && make && make install`
1. Make with code coverae enabled: `make clean && make enable_coverage=true && make install`. After running test, generate code coverage report: `make coverage-report`

Database restart is not required to catch up new container execution library,
you can simply connect to the database and all the calls you will make to
plcontainer language would be held by new binaries you just built


### Configuring PL/Container

You can start with the default configuration. To apply it do the following:
1. Login to Vagrant: `vagrant ssh`
1. Reset the configuration: `plcontainer configure --reset`

### Running the tests

1. Login to Vagrant: `vagrant ssh`
1. Go to the PL/Container test directory: `cd /plcontainer/tests`
1. Make it: `make tests`

### Design

The idea of PL/Container is to use containers to run user defined functions. The current implementation assume the PL function definition to have the following structure:

```sql
CREATE FUNCTION dummyPython() RETURNS text AS $$
# container: plc_python
return 'hello from Python'
$$ LANGUAGE plcontainer;
```

There are a couple of things that are interesting here:

1. The `LANGUAGE` argument to Greenplum is `plcontainer`

1. The function definition starts with the line `# container: plc_python` which defines the name of container that will be used for running this function. To check the list of containers defined in the system you can run the command `plcontainer configure --show`. Each container is mapped to a single docker image, you can list the ones available in your system with command `docker images`

1. At the moment two languages are supported for PL/Container: R and Python. Currently the official support images are still building. For an example of how to create a compatible docker image see `Dockerfile.R` and `Dockerfile.python`

1. The implementation assumes Docker container exposes some port, i.e. the container is started by an API call similar to running `docker run -d -P <image>` to publish the exposed port to a random port on the host.


For example, to define a function that uses a container that runs the `R`
interpreter, simply make the following definition:
```sql
CREATE FUNCTION dummyR() RETURNS text AS $$
# container: plc_r
return (log10(100))
$$ LANGUAGE plcontainer;
```


### Contributing
PL/Container is maintained by a core team of developers with commit rights to the [plcontainer repository](https://github.com/greenplum-db/plcontainer) on GitHub. At the same time, we are very eager to receive contributions and any discussions about it from anybody in the wider community.

Everyone interests PL/Container can [subscribe gpdb-dev](mailto:gpdb-dev+subscribe@greenplum.org) mailist list and send related topics to [gpdb-dev](mailto:gpdb-dev@greenplum.org).
