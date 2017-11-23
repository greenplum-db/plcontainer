Summary:        PL/Container for Greenplum database 
License:        Apache License, Version 2.0
Name:           plcontainer
Version:        %{plc_ver}
Release:        %{plc_rel}
Group:          Development/Tools
Prefix:         /temp
AutoReq:        no
AutoProv:       no
Provides:       plcontainer = %{plc_ver}

%description
Provides PL/Container procedural language implementation for the Greenplum Database.

%install
mkdir -p %{buildroot}/temp
make -C %{plc_dir} install DESTDIR=%{buildroot}/temp bindir=/bin libdir=/lib/postgresql pkglibdir=/lib/postgresql datadir=/share/postgresql
cp /usr/local/lib/libjson-c.so* %{buildroot}/temp/lib/postgresql/

%files
/temp
