## Run PL/Container For PostgreSQL

### Requirements

1. PL/Container runs on CentOS/RHEL 7.x or CentOS/RHEL 6.6+
1. PL/Container requires Docker version 17.05 for CentOS/RHEL 7.x and Docker version 1.7 CentOS/RHEL 6.6+
1. Postgresql version should be 9.6.0 or later

### Linux System
CentOS 7

### Install postgresql
```shell
#install postgresql's dependency
sudo yum groupinstall 'Development Tools'
sudo yum install readline-devel
sudo yum install zlib-devel

#install postgresql 9.6.9
git clone --branch REL9_6_9 git@github.com:postgres/postgres.git
cd postgres
git checkout -b REL9_6_9
./configure && make && make install

mkdir /usr/local/pgsql/data
sudo chown -R $USER:$USER /usr/local/pgsql
/usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data
Export PATH=/usr/local/pgsql/bin:$PATH
Export LD_LIBRARY_PATH=/usr/local/pgsql/lib:$LD_LIBRARY_PATH
pg_ctl -D /usr/local/pgsql/data -l pg.log start
```

### Install Docker  [Install Docker CE for CentOS](https://docs.docker.com/install/linux/docker-ce/centos/#install-docker-ce-1)
```shell
sudo yum install -y yum-utils device-mapper-persistent-data lvm2
sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
sudo yum install docker-ce
sudo systemctl start docker

#let plcontainer have permission to access docker
sudo groupadd docker
sudo chown root:docker /var/run/docker.sock
sudo usermod -a -G docker $USER
sudo systemctl stop docker.service
sudo systemctl start docker.service
```



### Prepare docker images for Python or R environment
Build from Dockerfile  [Refer](https://github.com/greenplum-db/plcontainer/wiki/How-to-build-docker-image)
```
cat << EOF >Dockerfile
FROM centos:7
EXPOSE 8080
EOF
docker build -f Dockerfile -t plcontainer_python_shared:devel ./
```

### Manual make a configuration for plcontainer extension
```shell
cat << EOF >/usr/local/pgsql/data/plcontainer_configuration.xml
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_shared</id>
        <image>plcontainer_python_shared:devel</image>
        <command>/clientdir/pyclient.sh</command>
        <shared_directory access="ro" container="/clientdir" host="/usr/local/pgsql/bin/plcontainer_clients"/>
    </runtime>
</configuration>
EOF
```

### Building PL/Container Language
Get the code repo
```shell
git clone https://github.com/greenplum-db/plcontainer.git
```

Install plcontainer’s dependency library
```shell
sudo yum install libcurl-devel
sudo yum install libxml2-devel
sudo yum install json-c-devel
sudo yum install python-devel
```

You can build PL/Container in the following way:

Set environment variable :
```shell
export PLC_PG=yes
export PGHOME=/usr/local/pgsql
export PGDATA=/usr/local/pgsql/data
export PGPORT=5432
```
1. Go to the PL/Container directory: `cd plcontainer`
1. plcontainer needs libcurl >=7.40. If the libcurl version on your system is low, you need to upgrade at first. For example, you could download source code and then compile and install, following this page: [Install libcurl from source](https://curl.haxx.se/docs/install.html). Note you should make sure the libcurl library path is in the list for library lookup. Typically you might want to add the path into LD_LIBRARY_PATH and export them in shell configuration or greenplum_path.sh on all nodes (Note you need to restart the Greenplum cluster).
1. Make and install it: `make clean && make && make install`

### Configuring PL/Container
To configure PL/Container environment, you need to enable PL/Container for specific databases by running 
   ```shell
   psql -d your_database -c 'create extension plcontainer;'
   ```

### Running the regression tests
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

select dummyPython();
```

### Contributing
PL/Container for postgresql now is a initial version , there are many features and issues to implement and fix, we are very eager to receive contributions and any discussions about it from anybody in the wider community.
